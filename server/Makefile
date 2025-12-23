CC = gcc
CFLAGS = -Iinclude -I../protocol -Wall 
LDFLAGS = -lsqlite3 -lpthread -lm   # Thêm thư viện SQLite, pthread, và math (cho cJSON)
# LDFLAGS means: Linker Flags, used to specify libraries to link against during the linking stage of compilation.

SRC = $(wildcard src/*.c src/handlers/*.c src/services/*.c src/utils/*.c ../protocol/frame.c ../protocol/connect.c ../protocol/cJSON.c)
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))
OBJ := $(patsubst src/handlers/%.c, build/handlers/%.o, $(OBJ))
OBJ := $(patsubst src/services/%.c, build/services/%.o, $(OBJ))
OBJ := $(patsubst ../protocol/%.c, build/protocol/%.o, $(OBJ))
OBJ := $(patsubst src/utils/%.c, build/utils/%.o, $(OBJ))

server: $(OBJ)
	$(CC) $(OBJ) -o server $(LDFLAGS)   # Link với SQLite

# Rule build file .c thành file .o trong build/
build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/utils/%.o: src/utils/%.c
	@mkdir -p build/utils
	$(CC) $(CFLAGS) -c $< -o $@
	
build/services/%.o: src/services/%.c
	@mkdir -p build/services
	$(CC) $(CFLAGS) -c $< -o $@

build/handlers/%.o: src/handlers/%.c
	@mkdir -p build/handlers
	$(CC) $(CFLAGS) -c $< -o $@

build/protocol/%.o: ../protocol/%.c
	@mkdir -p build/protocol
	$(CC) $(CFLAGS) -c $< -o $@

update_db:
	$(CC) $(CFLAGS) src/utils/update_db.c -o build/update_db $(LDFLAGS)
	./build/update_db

clean:
	rm -rf build server
