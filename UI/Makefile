CC = gcc
CFLAGS = -Wall -I../protocol `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -pthread

SRC = ui_main.c ../protocol/frame.c ../protocol/connect.c ../protocol/cJSON.c
OBJ = $(patsubst %.c, %.o, $(SRC))

ui_client: $(OBJ)
	$(CC) $(OBJ) -o ui_client $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) ui_client

.PHONY: clean
