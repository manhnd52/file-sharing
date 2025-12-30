CC := gcc

# ===== Flags =====
CFLAGS := -Wall -g  -O0 -Wextra -fPIC -Iinclude -I../protocol
DEPFLAGS := -MMD -MP

LDFLAGS :=
LDLIBS := -lpthread

# ===== Directories =====
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep

# ===== Sources =====
CLIENT_SRC := \
	$(wildcard src/*.c) \
	$(wildcard src/api/*.c) \
	$(wildcard src/utils/*.c)

PROTO_SRC := $(wildcard ../protocol/*.c)

SRC := $(CLIENT_SRC) $(PROTO_SRC)

# ===== Objects & Deps =====
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
DEP := $(patsubst %.c,$(DEP_DIR)/%.d,$(SRC))

# ===== Targets =====
all: libfsclient.so client

libfsclient.so: $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

client: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# ===== Compile rule =====
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@) $(DEP_DIR)/$(dir $*)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# ===== Include dependencies =====
-include $(DEP)

# ===== Utilities =====
clean:
	rm -rf $(BUILD_DIR) libfsclient.so client

run_test: client
	./client --test

.PHONY: all clean run_test
