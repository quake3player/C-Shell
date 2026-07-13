# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm
INCLUDE = -Iinclude

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OUT_DIR = .

# Output binary
TARGET = $(OUT_DIR)/shell.out

# Source and object files (recursive search)
SOURCES := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c)
OBJECTS := $(SOURCES:.c=.o)


# Default target
all: $(TARGET)

# Link object files to create the binary
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(CFLAGS)

# Compile source files to object files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Clean up object files and binary
clean:
	rm -f $(OBJECTS) $(TARGET)


# Phony targets
.PHONY: all clean

