CC:=$(shell command -v musl-gcc 2>/dev/null || command -v gcc 2>/dev/null || command -v clang 2>/dev/null)
CFLAGS=-static -no-pie -g
BIN=bfelfx64

ifeq ($(strip $(CC)),)
CC=cc
endif

all: $(BIN)

$(BIN): %: %.c
	$(CC) -o $@ $< $(CFLAGS)

install:
	cp $(BIN) /usr/bin/$(BIN)

clean:
	rm -f $(BIN)
