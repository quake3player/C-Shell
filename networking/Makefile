# Compiler and flags
CC = gcc
CFLAGS = -Wall -g # -Wall enables all warnings, -g adds debug info

# --- OpenSSL Crypto Library Flags ---
# LDLIBS is the correct variable for libraries like -lcrypto
LDLIBS = -lcrypto

# Target executables
TARGETS = server client

# Default rule
all: $(TARGETS)

# Rule to build the server
# Added $(LDLIBS) to the end of the linking command
server: server.o sham-utils.o
	$(CC) $(CFLAGS) -o server server.o sham-utils.o $(LDLIBS)

# Rule to build the client
# Added $(LDLIBS) to the end of the linking command
client: client.o sham-utils.o
	$(CC) $(CFLAGS) -o client client.o sham-utils.o $(LDLIBS)

# Rule to compile .c files into .o files (This rule is correct)
# Note: I'm assuming your file is named sham-utils.c based on the .o file names
%.o: %.c sham.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f *.o $(TARGETS) server_log.txt client_log.txt received.txt received_lossy.txt received_flow.c received_stress.c dev_log.txt