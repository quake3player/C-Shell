#define _POSIX_C_SOURCE 200809L
#include "input/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    TOK_NAME,
    TOK_PIPE,
    TOK_SEMICOLON,
    TOK_AMPERSAND,
    TOK_INPUT,
    TOK_OUTPUT,
    TOK_APPEND,
    TOK_END,
    TOK_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

typedef struct {
    Token *tokens;
    size_t pos;
    size_t size;
} TokenStream;

/* ---------------- TOKENIZER ---------------- */

static Token *tokenize(const char *line, size_t *count) {
    Token *tokens = NULL;
    size_t cap = 0, len = 0;
    const char *p = line;

    while (*p) {
        while (isspace((unsigned char)*p)) p++;

        if (*p == '\0') break;

        Token tok = {TOK_INVALID, NULL};

        if (*p == '|') { tok.type = TOK_PIPE; p++; }
        else if (*p == ';') { tok.type = TOK_SEMICOLON; p++; }
        else if (*p == '&') { tok.type = TOK_AMPERSAND; p++; }
        else if (*p == '<') { tok.type = TOK_INPUT; p++; }
        else if (*p == '>') {
            if (*(p+1) == '>') { tok.type = TOK_APPEND; p += 2; }
            else { tok.type = TOK_OUTPUT; p++; }
        }
        else {
            // NAME
            const char *start = p;
            while (*p && !isspace((unsigned char)*p) &&
                   *p!='|' && *p!=';' && *p!='&' && *p!='<' && *p!='>') {
                p++;
            }
            size_t n = p - start;
            char *val = malloc(n+1);
            strncpy(val, start, n);
            val[n] = '\0';
            tok.type = TOK_NAME;
            tok.value = val;
        }

        if (len == cap) {
            cap = cap ? cap*2 : 8;
            tokens = realloc(tokens, cap * sizeof(Token));
        }
        tokens[len++] = tok;
    }

    // End marker
    Token endtok = {TOK_END, NULL};
    if (len == cap) {
        cap = cap ? cap*2 : 8;
        tokens = realloc(tokens, cap * sizeof(Token));
    }
    tokens[len++] = endtok;

    *count = len;
    return tokens;
}

/* ---------------- PARSER (recursive descent) ---------------- */

static Token *peek(TokenStream *ts) {
    if (ts->pos < ts->size) return &ts->tokens[ts->pos];
    return NULL;
}

static Token *consume(TokenStream *ts, TokenType type) {
    Token *t = peek(ts);
    if (t && t->type == type) {
        ts->pos++;
        return t;
    }
    return NULL;
}

static int parse_name(TokenStream *ts) {
    return consume(ts, TOK_NAME) != NULL;
}

static int parse_input(TokenStream *ts) {
    Token *t = peek(ts);
    if (t && t->type == TOK_INPUT) {
        ts->pos++;
        return parse_name(ts);
    }
    return 0;
}

static int parse_output(TokenStream *ts) {
    Token *t = peek(ts);
    if (t && (t->type == TOK_OUTPUT || t->type == TOK_APPEND)) {
        ts->pos++;
        return parse_name(ts);
    }
    return 0;
}

static int parse_atomic(TokenStream *ts) {
    if (!parse_name(ts)) return 0;

    while (1) {
        if (peek(ts)->type == TOK_NAME) { ts->pos++; }
        else if (parse_input(ts)) {}
        else if (parse_output(ts)) {}
        else break;
    }
    return 1;
}

static int parse_cmd_group(TokenStream *ts) {
    if (!parse_atomic(ts)) return 0;
    while (consume(ts, TOK_PIPE)) {
        if (!parse_atomic(ts)) return 0;
    }
    return 1;
}

// *** THIS IS THE FIXED FUNCTION ***
static int parse_shell_cmd(TokenStream *ts) {
    // A shell command must start with at least one command group.
    if (!parse_cmd_group(ts)) return 0;

    // Loop through any number of commands separated by '&' or ';'.
    while (peek(ts)->type == TOK_SEMICOLON || peek(ts)->type == TOK_AMPERSAND) {
        
        // ** THE FIX IS HERE **
        // Before consuming the separator, peek ahead. If the token *after*
        // the separator is the end of the input, then it's a valid trailing
        // separator. We break the loop and let the final check handle it.
        if (ts->pos + 1 < ts->size && ts->tokens[ts->pos + 1].type == TOK_END) {
            break;
        }

        ts->pos++; // Consume the separator ('&' or ';').

        // After a separator that is NOT at the end, another command group is REQUIRED.
        // If it's missing, that's a syntax error (e.g., "ls ; ; pwd").
        if (!parse_cmd_group(ts)) return 0;
    }

    // After the loop, we might have a trailing '&' or ';'. The grammar specifically
    // allows a final '&'. We will also allow a final ';' for robustness.
    consume(ts, TOK_AMPERSAND);
    consume(ts, TOK_SEMICOLON);

    // The command is valid only if we have now reached the end of the token stream.
    return peek(ts)->type == TOK_END;
}

/* ---------------- PUBLIC ---------------- */

int parse_command(const char *line) {
    size_t count;
    Token *toks = tokenize(line, &count);
    TokenStream ts = {toks, 0, count};

    int ok = parse_shell_cmd(&ts);

    // free allocated NAME values
    for (size_t i=0; i<count; i++) free(toks[i].value);
    free(toks);

    return ok;
}