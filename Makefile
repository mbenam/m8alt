CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
LDFLAGS = -lpthread -lserialport -levdev

# Note: src/fonts/fonts.c is expected to exist from the original source
SRC = src/main.c \
      src/config.c \
      src/command.c \
      src/ini.c \
      src/input.c \
      src/render.c \
      src/backends/m8_libserialport.c \
      src/backends/queue.c \
      src/backends/slip.c \
      src/fonts/fonts.c

OBJ = $(SRC:.c=.o)
TARGET = m8c_rpi

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
