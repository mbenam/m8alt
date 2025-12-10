CC = gcc
CFLAGS = -Wall -O3 -Isrc
LDFLAGS = -lm

SRC_DIR = src
OBJ_DIR = obj

# List all your C files here
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/display.c \
       $(SRC_DIR)/input.c \
       $(SRC_DIR)/serial.c \
       $(SRC_DIR)/ini.c \
       $(SRC_DIR)/slip.c

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
