CC:=$(shell command -v musl-gcc 2>/dev/null || command -v gcc 2>/dev/null || command -v cc 2>/dev/null)
CFLAGS=-Wall -Wextra -std=c11 -g -static
INCLUDES=-Iinclude
BIN=bfelfx64

ifeq ($(strip $(CC)),)
CC=cc
endif

all: $(BIN)

$(BIN): %: %.c
	$(CC) -o $@ $< -static

install:
	cp $(BIN) /usr/bin/$(BIN)

clean:
	rm -f $(BIN)
