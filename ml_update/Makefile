BINPATH=/usr/bin

CC=$(BINPATH)/gcc
CFLAGS  = -g -Wall
CFLAGS += -I../lib
CFLAGS += -I../lib/mbedtls/include
CFLAGS += -I../lib/inih

vpath %.c ../lib

.PHONY: clean server client

all: server client

mbedtls:
	$(MAKE) -C ../lib/mbedtls

../lib/mbedtls/library/libmbedcrypto.a: mbedtls

server: ../lib/mbedtls/library/libmbedcrypto.a
	$(CC) $(CFLAGS) ml_update.c ../lib/dryos_hal.c ../lib/inih/ini.c ../lib/mbedtls/library/libmbedcrypto.a -o server

client: ../lib/mbedtls/library/libmbedcrypto.a
	$(CC) $(CFLAGS) client.c ../lib/dryos_hal.c ../lib/inih/ini.c ../lib/mbedtls/library/libmbedcrypto.a -o client

clean:
	rm -f server
	rm -f client

