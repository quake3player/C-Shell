#include "sham.h"

// This will point to our log file if logging is enabled.
// "static" means it's only visible inside this file.
static FILE *log_file = NULL;
static FILE *dev_log_file = NULL;

// Initializes the log file.

void log_init(const char *role) {
    char *log_env = getenv("RUDP_LOG");


    if (log_env != NULL && strcmp(log_env, "1") == 0) {
        char log_filename[32];
        sprintf(log_filename, "%s_log.txt", role);
        
        log_file = fopen(log_filename, "w");
        if (log_file == NULL) {
            perror("Error opening log file");
            exit(1);
        }
    }
}

// Initializes the dev log file.
void dev_log_init() {
    char *dev_log_env = getenv("DEV_LOG");
    if (dev_log_env != NULL && strcmp(dev_log_env, "1") == 0) {
        dev_log_file = fopen("dev_log.txt", "w"); // Use "w" for standard text mode
        if (dev_log_file == NULL) {
            perror("Error opening dev_log.txt");
        }
    }
}

// Writes a message to the dev log file.
void dev_log_event(const char *format, ...) {
    if (dev_log_file == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(dev_log_file, format, args);
    va_end(args);
    fprintf(dev_log_file, "\n"); // Automatically add a newline for readability
    fflush(dev_log_file);
}


// Writes a log message. It works just like printf.
void log_event(const char *format, ...) {
    // If the log file wasn't opened, just do nothing.
    if (log_file == NULL) {
        return;
    }

    // --- Get and format the timestamp ---
    struct timeval current_time;
    gettimeofday(&current_time, NULL); // Gets time with microsecond precision
    
    char time_string[40];
    // Formats the "YYYY-MM-DD HH:MM:SS" part
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localtime(&current_time.tv_sec));

    // Print the full timestamp like "[2025-09-09 10:30:00.123456] [LOG] "
    fprintf(log_file, "[%s.%06ld] [LOG] ", time_string, current_time.tv_usec);

    // --- Print the actual message ---
    va_list args;
    va_start(args, format);
    // vfprintf is like fprintf, but for when you have `...` arguments.
    // We need it to pass the user's arguments to the file.
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n"); // Add a newline at the end
    fflush(log_file);        // Force the line to be written to the file immediately
}

void log_close() {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

void dev_log_close() {
    if (dev_log_file != NULL) {
        fclose(dev_log_file);
        dev_log_file = NULL;
    }
}

// Takes header data and writes it into a buffer for sending.
void pack_header(uint8_t *buffer, uint32_t seq, uint32_t ack, uint16_t flags, uint16_t win) {
    // Convert numbers to "network byte order". Computers can have different
    // ways of storing numbers, and this makes sure they all agree.
    uint32_t seq_net = htonl(seq);   // host to network long (32-bit)
    uint32_t ack_net = htonl(ack);
    uint16_t flags_net = htons(flags); // host to network short (16-bit)
    uint16_t win_net = htons(win);

    // Copy the bytes into the buffer at the correct positions.
    // We use memcpy because it's a safe way to copy raw bytes.
    memcpy(buffer + 0, &seq_net, 4);  // First 4 bytes are sequence number
    memcpy(buffer + 4, &ack_net, 4);  // Next 4 bytes are ack number
    memcpy(buffer + 8, &flags_net, 2); // Next 2 bytes are flags
    memcpy(buffer + 10, &win_net, 2); // Last 2 bytes are window size
}

// Takes a buffer from the network and reads the header data from it.
void unpack_header(const uint8_t *buffer, uint32_t *seq, uint32_t *ack, uint16_t *flags, uint16_t *win) {
    // We will use temporary variables to read from the buffer first.
    uint32_t seq_net, ack_net;
    uint16_t flags_net, win_net;

    memcpy(&seq_net,   buffer + 0, 4);
    memcpy(&ack_net,   buffer + 4, 4);
    memcpy(&flags_net, buffer + 8, 2);
    memcpy(&win_net,   buffer + 10, 2);

    // Now, we check each pointer. If it's NOT NULL, we convert the value
    // from network to host order and store it.
    if (seq != NULL) {
        *seq = ntohl(seq_net);
    }
    if (ack != NULL) {
        *ack = ntohl(ack_net);
    }
    if (flags != NULL) {
        *flags = ntohs(flags_net);
    }
    if (win != NULL) {
        *win = ntohs(win_net);
    }
}