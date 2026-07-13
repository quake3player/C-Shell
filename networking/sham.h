#ifndef SHAM_H
#define SHAM_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>

// --- Constants ---
#define PAYLOAD_SIZE 1024
#define SHAM_HEADER_SIZE 12
#define MAX_PACKET_SIZE (PAYLOAD_SIZE + SHAM_HEADER_SIZE)
#define FIXED_WINDOW_PACKETS 10 
#define RETRANSMISSION_TIMEOUT_US 500000 // 500ms

// --- Header Flags ---
#define SYN  0x1
#define ACK  0x2
#define FIN  0x4
#define CHAT 0x8

// --- Structures ---

// A structure to hold information about a packet in the client's sending window
typedef struct {
    bool sent;
    bool acked;
    uint32_t seq_num; // Use uint32_t for sequence numbers
    int packet_size;
    struct timeval sent_time;
    char packet_data[MAX_PACKET_SIZE];
} client_window_packet;

// A node for the server's linked list to buffer out-of-order packets
struct buffered_packet {
    uint32_t seq_num;
    int len;
    uint8_t data[MAX_PACKET_SIZE];
    struct buffered_packet *next;
};

// --- Function Prototypes for all shared functions ---

// Logging functions from cham-utils.c
void log_init(const char *role);
void dev_log_init(void);
void dev_log_event(const char *format, ...);
void log_event(const char *format, ...);
void log_close(void);
void dev_log_close(void);

// Header packing/unpacking functions from cham-utils.c
void pack_header(uint8_t *buffer, uint32_t seq, uint32_t ack, uint16_t flags, uint16_t win);
void unpack_header(const uint8_t *buffer, uint32_t *seq, uint32_t *ack, uint16_t *flags, uint16_t *win);

#endif // SHAM_H