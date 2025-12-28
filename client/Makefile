CC = gcc
CFLAGS = -I../protocol -Iinclude -Wall
LDFLAGS = -lpthread

BUILD_DIR = build

CLIENT_SRC = $(wildcard src/*.c) $(wildcard src/api/*.c) $(wildcard src/utils/*.c)

PROTO_SRC = $(wildcard ../protocol/*.c)
SRC = $(sort $(CLIENT_SRC) $(PROTO_SRC))

define src2obj
$(BUILD_DIR)/$(subst /,_,$(subst ../,,$(patsubst %.c,%.o,$(1))))
endef

OBJ = $(foreach src,$(SRC),$(call src2obj,$(src)))

all: libclient.so

define make-obj-rule
$(call src2obj,$(1)): $(1)
	mkdir -p $(dir $$@)
	$(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach src,$(SRC),$(eval $(call make-obj-rule,$(src))))

clean:
	rm -f $(OBJ) client

run: client
	./client 127.0.0.1 5555

run_test: client
	./client --test

.PHONY: clean run run_test
