# Static musl build for ARM embedded systems
CC = musl-gcc

# Musl needs explicit path to kernel headers on Raspberry Pi OS
# The headers are in /usr/include but musl doesn't search there by default
CFLAGS = -Wall -O3 -Isrc -static -idirafter /usr/include -idirafter /usr/include/arm-linux-gnueabihf
LDFLAGS = -lm -static

SRC_DIR = src
OBJ_DIR = obj

# List all your C files here
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/display.c \
       $(SRC_DIR)/input.c \
       $(SRC_DIR)/serial.c \
       $(SRC_DIR)/ini.c \
       $(SRC_DIR)/slip.c \

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = m8alt

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
