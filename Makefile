# PS5 JailbreakStore payload build.
#
# Usage:
#   make                        # build <folder-name>.elf
#   make clean                  # remove build_ps5/ and the ELF
#   PS5_PAYLOAD_SDK=/path make  # use a specific SDK location
#   make V=1                    # print full compiler/linker commands
#   PS5_HOST=<ip> make deploy   # build + send to console via prospero-deploy

ifeq ($(V),1)
  Q :=
else
  Q := @
endif

PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk

ifeq ($(wildcard $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk),)
$(error PS5 payload SDK not found at "$(PS5_PAYLOAD_SDK)" -- set PS5_PAYLOAD_SDK=/path/to/sdk)
endif
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

PROJECT_NAME := $(notdir $(patsubst %/,%,$(CURDIR)))

BUILD_DIR := build_ps5
OBJ_DIR   := $(BUILD_DIR)/obj
BIN       := $(PROJECT_NAME).elf

COMMON_CFLAGS := -fplt -g -O0 -fno-inline -fvisibility-nodllstorageclass=hidden \
                  -fms-extensions -Wno-microsoft-string-literal-from-predefined \
                  -Isource/third_party -MMD -MP

MAIN_CFLAGS   := $(COMMON_CFLAGS) -Wall -Werror
SQLITE_CFLAGS := $(COMMON_CFLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_LOAD_EXTENSION

OBJS := $(OBJ_DIR)/main.o $(OBJ_DIR)/third_party/sqlite3.o

all: $(BIN)

source/icon0_png.h: icon0.png tools/gen_icon_header.py
	@echo "  GEN     $@"
	$(Q)python3 tools/gen_icon_header.py icon0.png $@ icon0_png

$(OBJ_DIR)/main.o: source/main.c source/icon0_png.h
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(Q)$(CC) $(MAIN_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/third_party/sqlite3.o: source/third_party/sqlite3.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(Q)$(CC) $(SQLITE_CFLAGS) -c -o $@ $<

$(BIN): $(OBJS)
	@echo "  LD      $@"
	$(Q)$(CC) -o $@ $(OBJS)

deploy: $(BIN)
	$(PS5_DEPLOY) $(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN) source/icon0_png.h

.PHONY: all clean deploy

# Use generated header dependencies when they exist.
-include $(OBJS:.o=.d)
