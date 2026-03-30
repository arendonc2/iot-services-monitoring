CC=gcc
CFLAGS=-Wall -Wextra -pthread -Iserver/include
SRC=$(wildcard server/src/*.c)
OUT=server_bin

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)