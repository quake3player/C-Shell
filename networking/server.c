#include "sham.h" 
#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <stdbool.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <time.h> 
#include <unistd.h> 
#include <openssl/md5.h> 
#include <sys/select.h> 

#define SERVER_BUFFER_SIZE 8192

int get_buffer_size(struct buffered_packet *head) { 
    int size = 0; 
    struct buffered_packet *current = head; 
    while (current != NULL) { 
        size += (current->len - SHAM_HEADER_SIZE); 
        current = current->next; 
    } 
    return size; 
} 

void print_md5(const char *filename) { 
    unsigned char c[MD5_DIGEST_LENGTH]; 
    FILE *inFile = fopen(filename, "rb"); 
    MD5_CTX mdContext; 
    int bytes; 
    unsigned char data[1024]; 
    if (inFile == NULL) { 
        log_event("MD5_ERROR: Cannot open file %s for checksum.", filename); 
        printf("%s can't be opened.\n", filename); 
        return; 
    } 
    MD5_Init(&mdContext); 
    log_event("MD5_INFO: Initialized MD5 context for %s.", filename); 
    while ((bytes = fread(data, 1, 1024, inFile)) != 0) { 
        MD5_Update(&mdContext, data, bytes); 
        log_event("MD5_INFO: Updated MD5 with %d bytes.", bytes); 
    } 
    MD5_Final(c, &mdContext); 
    log_event("MD5_INFO: Finalized MD5 calculation."); 
    char md5_string[MD5_DIGEST_LENGTH * 2 + 1]; 
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) { 
        sprintf(md5_string + (i*2), "%02x", c[i]); 
    } 
    md5_string[MD5_DIGEST_LENGTH * 2] = '\0'; 
    log_event("MD5_RESULT: %s", md5_string); 
    printf("MD5: %s\n", md5_string); 
    fclose(inFile); 
} 

int main(int argc, char *argv[]) { 
    if (argc < 2) { 
        fprintf(stderr, "USAGE: %s <port> [--chat]\n", argv[0]); 
        return 1; 
    } 

    int port = atoi(argv[1]); 
    bool chat_mode = false; 
    double loss_rate = 0.0; 

    for (int i = 2; i < argc; i++) { 
        if (strcmp(argv[i], "--chat") == 0) { 
            chat_mode = true; 
        } else { 
            loss_rate = atof(argv[i]); 
        } 
    } 

    log_init("server"); 
    dev_log_init(); 
    srand(time(NULL));

    if (loss_rate > 0.0) { 
        dev_log_event("[DEV MODE] Packet loss simulation enabled (rate: %.2f).", loss_rate); 
    } 

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0); 
    if (sock_fd < 0) { perror("Could not create socket"); return 1; } 
    
    struct sockaddr_in server_address; 
    memset(&server_address, 0, sizeof(server_address)); 
    server_address.sin_family = AF_INET; 
    server_address.sin_addr.s_addr = INADDR_ANY; 
    server_address.sin_port = htons(port); 
    
    if (bind(sock_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) { 
        perror("Could not bind to port"); 
        return 1; 
    } 
    printf("Server is listening on port %d...\n", port); 

    uint8_t buffer[MAX_PACKET_SIZE]; 
    struct sockaddr_in client_address; 
    socklen_t client_address_len = sizeof(client_address); 
    uint32_t client_initial_seq = 0; 
    uint32_t server_initial_seq = 0; 
    int bytes_received; 

    // Wait for handshake
    while (1) { 
        bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                                 (struct sockaddr *)&client_address, &client_address_len); 
        if (bytes_received <= 0) continue; 

        uint32_t seq; 
        uint16_t flags; 
        unpack_header(buffer, &seq, NULL, &flags, NULL); 

        if (flags & SYN) { 
            client_initial_seq = seq; 
            log_event("RCV SYN SEQ=%u", client_initial_seq); 
            server_initial_seq = rand(); 
            if (flags & CHAT) { 
                chat_mode = true; 
                log_event("CHAT requested by client during handshake"); 
            } 
            uint16_t synack_flags = SYN | ACK | (chat_mode ? CHAT : 0); 
            pack_header(buffer, server_initial_seq, client_initial_seq + 1, synack_flags, SERVER_BUFFER_SIZE); 
            sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
                   (struct sockaddr *)&client_address, client_address_len); 
            log_event("SND SYN-ACK SEQ=%u ACK=%u", server_initial_seq, client_initial_seq + 1); 
            printf("Handshake initiated! Ready to receive %s.\n", chat_mode ? "chat messages" : "data"); 
            break; 
        } 
    } 

    if (chat_mode) { 
        printf("[INFO] Server is in chat mode. You can now send messages.\n"); 
        log_event("ENTER CHAT MODE"); 
        uint32_t server_seq_num = server_initial_seq + 1;  // Track server's sequence number
        
        while (1) { 
            fd_set read_fds; 
            FD_ZERO(&read_fds); 
            FD_SET(sock_fd, &read_fds); 
            FD_SET(STDIN_FILENO, &read_fds); 
            int max_fd = (sock_fd > STDIN_FILENO) ? sock_fd : STDIN_FILENO; 
            
            int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL); 
            if (activity < 0) { 
                perror("select error"); 
                break; 
            } 
            
            // Handle incoming messages from client
            if (FD_ISSET(sock_fd, &read_fds)) { 
                bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                                        (struct sockaddr *)&client_address, &client_address_len); 
                if (bytes_received <= 0) { 
                    printf("Client disconnected.\n"); 
                    break; 
                } 
                
                uint32_t seq_num, ack_num; 
                uint16_t flags; 
                unpack_header(buffer, &seq_num, &ack_num, &flags, NULL); 
                
                // Handle handshake ACK
                if ((flags & ACK) && (ack_num == server_initial_seq + 1) && (bytes_received == SHAM_HEADER_SIZE)) { 
                    log_event("RCV ACK FOR SYN"); 
                    continue; 
                } 
                
                // Handle FIN
                if (flags & FIN) { 
                    log_event("RCV FIN SEQ=%u", seq_num); 
                    uint32_t ack_to_client = seq_num + 1; 
                    pack_header(buffer, server_seq_num, ack_to_client, FIN | ACK, 0); 
                    sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
                           (struct sockaddr *)&client_address, client_address_len); 
                    log_event("SND FIN-ACK"); 
                    
                    // Wait for final ACK
                    bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                                            (struct sockaddr *)&client_address, &client_address_len); 
                    if (bytes_received > 0) { 
                        unpack_header(buffer, &seq_num, &ack_num, &flags, NULL); 
                        if ((flags & ACK) && (ack_num > server_initial_seq + 1)) { 
                            log_event("RCV FINAL ACK=%u", ack_num); 
                        } 
                    } 
                    break; 
                } 
                
                // Handle data message from client
                if (bytes_received > SHAM_HEADER_SIZE) { 
                    char *chat_msg = (char *)(buffer + SHAM_HEADER_SIZE); 
                    int msg_len = bytes_received - SHAM_HEADER_SIZE; 
                    if (msg_len < (sizeof(buffer) - SHAM_HEADER_SIZE)) { 
                        chat_msg[msg_len] = '\0'; 
                    } 
                    log_event("RCV DATA SEQ=%u LEN=%d", seq_num, msg_len); 
                    printf("Client: %s", chat_msg); 
                    fflush(stdout);
                    // DO NOT ECHO BACK - just receive and display
                    // Remove the sendto() that was echoing the message back
                } 
            } 
            
            // Handle server's input from stdin
            if (FD_ISSET(STDIN_FILENO, &read_fds)) { 
                char stdin_buffer[PAYLOAD_SIZE]; 
                if (fgets(stdin_buffer, sizeof(stdin_buffer), stdin) == NULL) { 
                    printf("Shutting down due to stdin close.\n"); 
                    break; 
                } 
                
                // Send server's message to client
                int msg_len = strlen(stdin_buffer);
                pack_header(buffer, server_seq_num, 0, 0, 0); 
                memcpy(buffer + SHAM_HEADER_SIZE, stdin_buffer, msg_len + 1); 
                sendto(sock_fd, buffer, SHAM_HEADER_SIZE + msg_len + 1, 0, 
                       (struct sockaddr *)&client_address, client_address_len); 
                log_event("SND DATA SEQ=%u LEN=%d", server_seq_num, msg_len); 
                server_seq_num += msg_len;  // Update sequence number
            } 
        } 
    } else { 
        // File transfer mode (unchanged)
        printf("[INFO] Server is in file transfer mode.\n"); 
        log_event("ENTER FILE TRANSFER MODE"); 

        uint32_t expected_seq_num = client_initial_seq + 1; 
        bool filename_received = false; 
        bool fin_received = false;
        uint32_t fin_seq_num = 0; 
        FILE *output_file = NULL; 
        char output_filename[256] = {0}; 
        struct buffered_packet *buffered_list_head = NULL; 

        while (!fin_received || expected_seq_num < fin_seq_num) { 
            fd_set read_fds; 
            FD_ZERO(&read_fds); 
            FD_SET(sock_fd, &read_fds); 
            struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 }; 
            int activity = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout); 
            
            if (activity < 0) { 
                perror("select error"); 
                break; 
            } 
            if (activity == 0) { 
                if (fin_received) { 
                    log_event("TERMINATION_ERROR: Timeout waiting for missing packets after FIN received."); 
                    break; 
                } 
                continue; 
            } 

            bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, 
                                    (struct sockaddr *)&client_address, &client_address_len); 
            if (bytes_received <= 0) continue; 

            if (loss_rate > 0 && (double)rand() / RAND_MAX < loss_rate) { 
                uint32_t dropped_seq; 
                unpack_header(buffer, &dropped_seq, NULL, NULL, NULL); 
                dev_log_event("[DEV MODE] Dropping packet (SEQ=%u).", dropped_seq); 
                continue; 
            } 

            uint32_t seq_num; 
            uint16_t flags; 
            unpack_header(buffer, &seq_num, NULL, &flags, NULL); 
            int payload_len = bytes_received - SHAM_HEADER_SIZE; 

            if ((flags & ACK) && payload_len == 0) { 
                log_event("RCV ACK FOR SYN"); 
                continue; 
            } 

            if (flags & FIN) { 
                if (!fin_received) { 
                    log_event("RCV FIN SEQ=%u", seq_num); 
                    fin_received = true; 
                    fin_seq_num = seq_num; 
                } 
            } else if (payload_len > 0) { 
                log_event("RCV DATA SEQ=%u LEN=%d", seq_num, payload_len); 

                if (seq_num == expected_seq_num) {
                    if (!filename_received) { 
                        strncpy(output_filename, (char *)(buffer + SHAM_HEADER_SIZE), sizeof(output_filename) - 1);
                        output_filename[sizeof(output_filename) - 1] = '\0';
                        
                        output_file = fopen(output_filename, "wb"); 
                        if (!output_file) { 
                            perror("Could not create output file"); 
                            close(sock_fd); 
                            return 1; 
                        } 
                        filename_received = true; 
                        log_event("INFO Filename received: %s", output_filename); 
                        expected_seq_num += payload_len; 
                    } else { 
                        fwrite(buffer + SHAM_HEADER_SIZE, 1, payload_len, output_file); 
                        expected_seq_num += payload_len; 
                        
                        while (buffered_list_head != NULL && buffered_list_head->seq_num == expected_seq_num) { 
                            dev_log_event("[DEV MODE] Writing buffered packet (SEQ=%u).", buffered_list_head->seq_num); 
                            int buffered_payload_len = buffered_list_head->len - SHAM_HEADER_SIZE; 
                            fwrite(buffered_list_head->data + SHAM_HEADER_SIZE, 1, buffered_payload_len, output_file); 
                            expected_seq_num += buffered_payload_len; 
                            struct buffered_packet *to_free = buffered_list_head; 
                            buffered_list_head = buffered_list_head->next; 
                            free(to_free); 
                        } 
                    }
                } else if (seq_num > expected_seq_num) { 
                    dev_log_event("[DEV MODE] Out-of-order packet (SEQ=%u), expected (%u). Buffering.", seq_num, expected_seq_num); 
                    struct buffered_packet *new_node = (struct buffered_packet *)malloc(sizeof(struct buffered_packet)); 
                    new_node->seq_num = seq_num; 
                    new_node->len = bytes_received; 
                    memcpy(new_node->data, buffer, bytes_received); 
                    
                    if (buffered_list_head == NULL || buffered_list_head->seq_num > seq_num) { 
                        new_node->next = buffered_list_head; 
                        buffered_list_head = new_node; 
                    } else { 
                        struct buffered_packet *curr = buffered_list_head; 
                        while (curr->next != NULL && curr->next->seq_num < seq_num) { 
                            curr = curr->next; 
                        } 
                        if (curr->seq_num != seq_num) { 
                            new_node->next = curr->next; 
                            curr->next = new_node; 
                        } else { 
                            free(new_node); 
                        } 
                    } 
                } 
            } 
            
            pack_header(buffer, 0, expected_seq_num, ACK, SERVER_BUFFER_SIZE - get_buffer_size(buffered_list_head)); 
            sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
                   (struct sockaddr *)&client_address, client_address_len); 
            log_event("SND ACK=%u", expected_seq_num); 
        } 

        log_event("All data received up to FIN. Initiating termination."); 
        if (output_file) { 
            fclose(output_file); 
            log_event("INFO File %s closed.", output_filename); 
            printf("File transfer complete. Calculating MD5...\n"); 
            print_md5(output_filename); 
        } 

        uint32_t server_fin_seq = server_initial_seq + 1; 
        pack_header(buffer, server_fin_seq, fin_seq_num + 1, FIN | ACK, 0); 
        int retries = 5; 
        while (retries-- > 0) { 
            sendto(sock_fd, buffer, SHAM_HEADER_SIZE, 0, 
                   (struct sockaddr *)&client_address, client_address_len); 
            log_event("SND FIN-ACK SEQ=%u ACK=%u", server_fin_seq, fin_seq_num + 1); 
            
            fd_set read_fds; 
            FD_ZERO(&read_fds); 
            FD_SET(sock_fd, &read_fds); 
            struct timeval final_ack_timeout = { .tv_sec = 0, .tv_usec = RETRANSMISSION_TIMEOUT_US }; 
            int activity = select(sock_fd + 1, &read_fds, NULL, NULL, &final_ack_timeout); 
            
            if (activity > 0) { 
                bytes_received = recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL); 
                if (bytes_received > 0) { 
                    uint32_t ack_num; 
                    uint16_t flags; 
                    unpack_header(buffer, NULL, &ack_num, &flags, NULL); 
                    if ((flags & ACK) && (ack_num == server_fin_seq + 1)) { 
                        log_event("RCV FINAL ACK FOR FIN-ACK"); 
                        break; 
                    } 
                } 
            } else if (activity < 0) { 
                perror("select error"); 
                break; 
            } 
        } 
        
        if (retries < 0) { 
            log_event("TERMINATION_ERROR: Did not receive final ACK from client."); 
        } 

        struct buffered_packet *curr = buffered_list_head; 
        while (curr != NULL) { 
            struct buffered_packet *temp = curr; 
            curr = curr->next; 
            free(temp); 
        } 
        printf("Server shutting down file transfer session.\n"); 
    } 

    close(sock_fd); 
    log_event("INFO Socket closed."); 
    log_close();
    dev_log_close();
    return 0; 
}