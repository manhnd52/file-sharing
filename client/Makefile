CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -lpthread

SRC = ../protocol/frame.c ../protocol/connect.c ../protocol/cJSON.c client.c
OBJ = ../protocol/frame.pic.o ../protocol/connect.pic.o ../protocol/cJSON.pic.o client.pic.o

all: libclient.so

libclient.so: $(OBJ)
	$(CC) -shared -o $@ $(OBJ) $(LDFLAGS)

client.pic.o: client.c
	$(CC) $(CFLAGS) -c $< -o $@

../protocol/%.pic.o: ../protocol/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) libclient.so

.PHONY: all clean
