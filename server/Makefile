CC = gcc
CFLAGS = -Iinclude -I../protocol -Wall
LDFLAGS = -lsqlite3   # Thêm thư viện SQLite
# LDFLAGS means: Linker Flags, used to specify libraries to link against during the linking stage of compilation.

SRC = $(wildcard src/*.c src/handlers/*.c src/services/*.c)
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))
OBJ := $(patsubst src/handlers/%.c, build/handlers/%.o, $(OBJ))
OBJ := $(patsubst src/services/%.c, build/services/%.o, $(OBJ))

server: $(OBJ)
	$(CC) $(OBJ) -o server $(LDFLAGS)   # Link với SQLite

# Rule build file .c thành file .o trong build/
build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/services/%.o: src/services/%.c
	@mkdir -p build/services
	$(CC) $(CFLAGS) -c $< -o $@


build/handlers/%.o: src/handlers/%.c
	@mkdir -p build/handlers
	$(CC) $(CFLAGS) -c $< -o $@

update_db:
	$(CC) $(CFLAGS) src/utils/update_db.c -o build/update_db $(LDFLAGS)
	./build/update_db

clean:
	rm -rf build server
