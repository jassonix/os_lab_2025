CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -pthread

COMMON_OBJS = common.o
LIBCOMMON = libcommon.a

all: client server

$(LIBCOMMON): $(COMMON_OBJS)
	ar rcs $@ $

client: client.o $(LIBCOMMON)
	$(CC) $(CFLAGS) -o $@ client.o $(LIBCOMMON) $(LDFLAGS)

server: server.o $(LIBCOMMON)
	$(CC) $(CFLAGS) -o $@ server.o $(LIBCOMMON) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(LIBCOMMON) client server

.PHONY: all clean
