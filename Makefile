# Static musl build for ARM embedded systems
CC = musl-gcc

# AUDIO_CORE Configuration:
# Set to 'none' for single-core or OS-managed scheduling.
# Set to 3 to pin audio thread to Core 3 (Best for Zero 2W / Pi 3).
AUDIO_CORE = none

# Musl needs explicit path to kernel headers on Raspberry Pi OS
# -Isrc/tinyalsa allows pcm.c to find asoundlib.h
CFLAGS = -Wall -O3 -Isrc -static -idirafter /usr/include -idirafter /usr/include/arm-linux-gnueabihf
LDFLAGS = -lm -lpthread -static

# Handle Audio Pinning Flag
ifneq ($(AUDIO_CORE), none)
    CFLAGS += -DAUDIO_PIN_CORE=$(AUDIO_CORE)
endif

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/display.c \
       $(SRC_DIR)/input.c \
       $(SRC_DIR)/serial.c \
       $(SRC_DIR)/ini.c \
       $(SRC_DIR)/slip.c \
       $(SRC_DIR)/audio.c \
       $(SRC_DIR)/pcm.c

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
