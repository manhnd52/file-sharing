CC = gcc
CFLAGS = -I../protocol -Iinclude -Wall
LDFLAGS = -lpthread

SRC = client.c ../protocol/frame.c ../protocol/connect.c ../protocol/cJSON.c
SRC_TEST = client_test.c ../protocol/frame.c ../protocol/connect.c ../protocol/cJSON.c
OBJ = $(patsubst %.c, %.o, $(SRC))
OBJ_TEST = $(patsubst %.c, %.o, $(SRC_TEST))

client: $(OBJ)
	$(CC) $(OBJ) -o client $(LDFLAGS)

client_test: $(OBJ_TEST)
	$(CC) $(OBJ_TEST) -o client_test $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OBJ_TEST) client client_test

run: client
	./client 127.0.0.1 5555

run_test: client_test
	./client_test 127.0.0.1 5555

.PHONY: clean run run_test
