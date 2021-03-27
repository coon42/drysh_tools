BINPATH=/usr/bin

CC=$(BINPATH)/gcc
CFLAGS  = -g -Wall
CFLAGS += -I../lib

vpath %.c ../lib

.PHONY: clean server client

all: server client

server:
	$(CC) $(CFLAGS) ml_update.c ../lib/dryos_hal.c ../lib/mbedtls/library/libmbedcrypto.a -o server

client:
	$(CC) $(CFLAGS) client.c ../lib/dryos_hal.c ../lib/mbedtls/library/libmbedcrypto.a -o client

clean:
	rm -f server
	rm -f client

