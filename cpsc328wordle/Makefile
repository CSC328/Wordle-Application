CC = gcc
AR = ar
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -I.
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
ARFLAGS = rcs
LDFLAGS =
LDLIBS =

.PHONY: all library programs test sanitize clean help

# At this stage, the completed product is the shared library.
# Explicit client/server targets are ready for each teammate's implementation.
all: library

library: libwordle.a

programs: client server

libwordle.a: wordle_common.o
	$(AR) $(ARFLAGS) $@ $^

wordle_common.o: wordle_common.c wordle_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

client.o: client.c wordle_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

server.o: server.c wordle_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

client: client.o libwordle.a
	$(CC) $(LDFLAGS) -o $@ client.o -L. -lwordle $(LDLIBS)

server: server.o libwordle.a
	$(CC) $(LDFLAGS) -o $@ server.o -L. -lwordle $(LDLIBS)

tests/test_common: tests/test_common.c libwordle.a wordle_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_common.c \
		-L. -lwordle -Wl,--wrap=send -Wl,--wrap=recv $(LDLIBS)

test: tests/test_common
	./tests/test_common

tests/test_common_sanitize: tests/test_common.c wordle_common.c wordle_common.h
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-o $@ tests/test_common.c wordle_common.c \
		-Wl,--wrap=send -Wl,--wrap=recv

sanitize: tests/test_common_sanitize
	./tests/test_common_sanitize

clean:
	rm -f *.o libwordle.a client server tests/test_common tests/test_common_sanitize

help:
	@printf '%s\n' 'make: build libwordle.a' \
		'make test: run the shared-library tests' \
		'make sanitize: run address/undefined-behavior checks' \
		'make programs: build client/server (currently starters)' \
		'make clean: remove generated build files'
