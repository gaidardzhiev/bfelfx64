CC:=$(shell command -v musl-gcc 2>/dev/null || command -v gcc 2>/dev/null || command -v cc 2>/dev/null)
INCLUDES=-Iinclude
BIN=bfelfx64

ifeq ($(strip $(CC)),)
CC=cc
endif

all: $(BIN)

$(BIN): %: %.c
	$(CC) -o $@ $< -static -no-pie -g

install:
	cp $(BIN) /usr/bin/$(BIN)

clean:
	rm -f $(BIN)
