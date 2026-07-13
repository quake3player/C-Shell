#include "sham.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "USAGE:\n  File Transfer: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n  Chat:          %s <server_ip> <server_port> --chat\n", argv[0], argv[0]);
        return 1;
    }

    const char *server_ip_str = argv[1];
    int server_port = atoi(argv[2]);
    const char *input_filename = NULL;
    const char *output_filename = NULL;
    bool chat_mode = false;
    double loss_rate = 0.0;

    int current_arg = 3;
    if (strcmp(argv[current_arg], "--chat") == 0) {
        chat_mode = true;
        current_arg++;
    } else {
        if (argc < 5) {
            fprintf(stderr, "Missing file arguments for File Transfer mode.\n");
            return 1;
        }
        input_filename = argv[current_arg++];
        output_filename = argv[current_arg++];
    }

    if (current_arg < argc) {
        loss_rate = atof(argv[current_arg]);
    }

    log_init("client");
    dev_log_init();
    srand(time(NULL));

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) { perror("Could not create socket"); return 1; }
    
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip_str);
    
    // --- Handshake ---
    uint8_t buffer[MAX_PACKET_SIZE];
    uint32_t client_initial_seq = rand();
    uint32_t server_initial_seq = 0;

    uint16_t initial_flags = SYN | (chat_mode ? CHAT : 0);
    pack_header(buffer, client_initial_seq, 0, initial_flags, 0);
    sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, (struct sockaddr *)&server_address, sizeof(server_address));
    log_event("SND SYN SEQ=%u", client_initial_seq);

    int bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
    uint16_t server_window_size = 0;
    if (bytes_received > 0) {
        uint32_t seq, ack;
        uint16_t flags;
        unpack_header(buffer, &seq, &ack, &flags, &server_window_size);
        if ((flags & SYN) && (flags & ACK) && (ack == client_initial_seq + 1)) {
            server_initial_seq = seq;
            pack_header(buffer, client_initial_seq + 1, server_initial_seq + 1, ACK, 0);
            sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, (struct sockaddr *)&server_address, sizeof(server_address));
            log_event("SND ACK FOR SYN-ACK");
            printf("Handshake successful! You can now %s.\n", chat_mode ? "chat" : "transfer a file");
        }
    }

    if (chat_mode) {
        uint32_t client_seq_num = client_initial_seq + 1;
        while (1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sock_fd, &read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            int max_fd = (sock_fd > STDIN_FILENO) ? sock_fd : STDIN_FILENO;

            if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
                perror("select error");
                break;
            }

            if (FD_ISSET(sock_fd, &read_fds)) {
                struct sockaddr_in packet_source;
                socklen_t source_len = sizeof(packet_source);
                bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                                         (struct sockaddr *)&packet_source, &source_len);

                if (bytes_received <= 0) {
                    printf("Server disconnected.\n");
                    break;
                }

                // CRITICAL FIX: Filter out our own packets
                // Compare both IP address AND port to ensure packet is from server
                if (packet_source.sin_addr.s_addr != server_address.sin_addr.s_addr ||
                    packet_source.sin_port != server_address.sin_port) {
                    dev_log_event("Ignoring packet from unexpected source (not from server)");
                    continue;  // Skip this packet - it's not from the server
                }

                uint32_t seq, ack;
                uint16_t flags;
                unpack_header(buffer, &seq, &ack, &flags, NULL);

                if (flags == (FIN | ACK)) {
                    pack_header(buffer, ack, seq + 1, ACK, 0);
                    sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
                           (struct sockaddr *)&server_address, sizeof(server_address));
                    log_event("SND FINAL ACK FOR FIN");
                    break;
                }

                if (bytes_received > SHAM_HEADER_SIZE) {
                    char *msg = (char *)(buffer + SHAM_HEADER_SIZE);
                    int msg_len = bytes_received - SHAM_HEADER_SIZE;
                    msg[msg_len] = '\0';  // Properly null-terminate
                    printf("Server: %s", msg);
                    fflush(stdout);
                }
            }

            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                char stdin_buffer[PAYLOAD_SIZE];
                if (fgets(stdin_buffer, sizeof(stdin_buffer), stdin) == NULL) break;

                if (strcmp(stdin_buffer, "/quit\n") == 0) {
                    pack_header(buffer, client_seq_num, 0, FIN, 0);
                    sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
                           (struct sockaddr *)&server_address, sizeof(server_address));
                    log_event("SND FIN SEQ=%u", client_seq_num);
                    continue;
                }
                
                int msg_len = strlen(stdin_buffer);
                pack_header(buffer, client_seq_num, 0, 0, 0);
                memcpy(buffer + SHAM_HEADER_SIZE, stdin_buffer, msg_len + 1);
                sendto(sock_fd, buffer, SHAM_HEADER_SIZE + msg_len + 1, 0, 
                       (struct sockaddr *)&server_address, sizeof(server_address));
                log_event("SND DATA SEQ=%u LEN=%d", client_seq_num, msg_len);
                client_seq_num += msg_len;
            }
        }
    } else {
        // File Transfer Logic (unchanged)
        FILE *input_file = fopen(input_filename, "rb");
        if (input_file == NULL) { 
            perror("Failed to open input file"); 
            close(sock_fd); 
            return 1; 
        }

        client_window_packet window[FIXED_WINDOW_PACKETS];
        memset(window, 0, sizeof(window));

        uint32_t send_base = client_initial_seq + 1;
        uint32_t next_seq_to_send = client_initial_seq + 1;
        bool file_is_done = false;

        // Send filename
        int filename_len = strlen(output_filename) + 1;
        pack_header(buffer, next_seq_to_send, 0, 0, 0);
        memcpy(buffer + SHAM_HEADER_SIZE, output_filename, filename_len);
        
        while (true) {
            sendto(sock_fd, buffer, SHAM_HEADER_SIZE + filename_len, 0, 
                   (struct sockaddr *)&server_address, sizeof(server_address));
            log_event("SND DATA SEQ=%u (filename)", next_seq_to_send);
            
            fd_set read_fds; 
            FD_ZERO(&read_fds); 
            FD_SET(sock_fd, &read_fds);
            struct timeval timeout = { .tv_sec = 0, .tv_usec = RETRANSMISSION_TIMEOUT_US };
            
            if (select(sock_fd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
                bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
                uint32_t ack_num;
                unpack_header(buffer, NULL, &ack_num, NULL, &server_window_size);
                if (ack_num >= next_seq_to_send + filename_len) {
                     send_base = ack_num;
                     next_seq_to_send = ack_num;
                     break;
                }
            }
        }

        while (!file_is_done || send_base < next_seq_to_send) {
            if (!file_is_done) {
                for (int i = 0; i < FIXED_WINDOW_PACKETS; i++) {
                    if (!window[i].sent && (next_seq_to_send - send_base < server_window_size)) {
                        int bytes_read = fread(window[i].packet_data + SHAM_HEADER_SIZE, 1, 
                                             PAYLOAD_SIZE, input_file);
                        if (bytes_read <= 0) { 
                            file_is_done = true; 
                            break; 
                        }
                        
                        window[i].seq_num = next_seq_to_send;
                        window[i].packet_size = SHAM_HEADER_SIZE + bytes_read;
                        pack_header((uint8_t *)window[i].packet_data, window[i].seq_num, 0, 0, 0);
                        
                        window[i].sent = true;
                        window[i].acked = false;
                        gettimeofday(&window[i].sent_time, NULL);

                        sendto(sock_fd, window[i].packet_data, window[i].packet_size, 0, 
                               (struct sockaddr *)&server_address, sizeof(server_address));
                        log_event("SND DATA SEQ=%u LEN=%d", window[i].seq_num, bytes_read);
                        next_seq_to_send += bytes_read;
                    }
                }
            }

            fd_set read_fds; 
            FD_ZERO(&read_fds); 
            FD_SET(sock_fd, &read_fds);
            struct timeval timeout = { .tv_sec = 0, .tv_usec = 10000 };
            
            if (select(sock_fd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
                bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
                if (bytes_received > 0) {
                    uint32_t ack_num;
                    unpack_header(buffer, NULL, &ack_num, NULL, &server_window_size);
                    log_event("RCV ACK=%u WIN=%u", ack_num, server_window_size);
                    if (ack_num > send_base) {
                        send_base = ack_num;
                        for (int i = 0; i < FIXED_WINDOW_PACKETS; i++) {
                            if (window[i].sent && !window[i].acked && 
                                window[i].seq_num < send_base) {
                                window[i].sent = false;
                                window[i].acked = true;
                            }
                        }
                    }
                }
            }

            struct timeval now; 
            gettimeofday(&now, NULL);
            for (int i = 0; i < FIXED_WINDOW_PACKETS; i++) {
                if (window[i].sent && !window[i].acked) {
                    long elapsed_us = (now.tv_sec - window[i].sent_time.tv_sec) * 1000000L + 
                                    (now.tv_usec - window[i].sent_time.tv_usec);
                    if (elapsed_us > RETRANSMISSION_TIMEOUT_US) {
                        log_event("TIMEOUT SEQ=%u", window[i].seq_num);
                        sendto(sock_fd, window[i].packet_data, window[i].packet_size, 0, 
                               (struct sockaddr *)&server_address, sizeof(server_address));
                        gettimeofday(&window[i].sent_time, NULL);
                    }
                }
            }
            if (file_is_done && send_base >= next_seq_to_send) break;
        }

        fclose(input_file);
        
        // Teardown
        uint32_t client_fin_seq = next_seq_to_send;
        pack_header(buffer, client_fin_seq, 0, FIN, 0);
        sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
               (struct sockaddr *)&server_address, sizeof(server_address));
        log_event("SND FIN SEQ=%u", client_fin_seq);
    }

    close(sock_fd);
    log_close();
    dev_log_close();
    printf("Client shutting down.\n");
    return 0;
}