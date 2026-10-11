CC       ?= gcc
AR       ?= ar
CSTD     ?= -std=c11

WARNINGS := -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-qual \
            -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter
ifeq ($(WERROR),1)
WARNINGS += -Werror
endif
MODE     ?= release
ifeq ($(MODE),debug)
OPT      ?= -O0 -g3 -DRVM_DEBUG
else ifeq ($(MODE),sanity)
OPT      ?= -O1 -g -fsanitize=address,undefined
else
OPT      ?= -O2
endif
DEFS     ?= -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS   ?= $(CSTD) $(DEFS) $(OPT) $(WARNINGS) -Isrc -fno-omit-frame-pointer
CFLAGS   += $(EXTRA_CFLAGS)
LDFLAGS  ?=
ifeq ($(MODE),sanity)
LDFLAGS  += -fsanitize=address,undefined
endif
LDFLAGS  += $(EXTRA_LDFLAGS)
LDLIBS   ?= -lm

BUILD    := build
BIN      := rvm
TEST_BIN := tests/rvm_tests

SRC      := $(sort $(shell find src -name '*.c'))
OBJ      := $(patsubst %.c,$(BUILD)/%.o,$(SRC))
DEP      := $(OBJ:.o=.d)

LIB_SRC  := $(filter-out src/main.c,$(SRC))
LIB_OBJ  := $(patsubst %.c,$(BUILD)/%.o,$(LIB_SRC))

TEST_SRC := $(sort $(shell find tests -name '*.c'))
TEST_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(TEST_SRC))

.PHONY: all test lint format clean run dtb coverage help objects
.DEFAULT_GOAL := all

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "built $@ ($$(du -h $@ 2>/dev/null | cut -f1))"

$(BUILD)/librvm.a: $(LIB_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

lib: $(BUILD)/librvm.a

TEST_ARGS ?=

test: $(TEST_BIN)
	./$(TEST_BIN) $(TEST_ARGS)

$(TEST_BIN): $(TEST_OBJ) $(BUILD)/librvm.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TEST_OBJ) $(BUILD)/librvm.a $(LDLIBS)

lint:
	$(CC) $(CSTD) $(DEFS) $(WARNINGS) -Werror -Isrc -fsyntax-only $(SRC) $(TEST_SRC)
	@echo "lint: OK"

format:
	@command -v clang-format >/dev/null || { echo "clang-format not installed"; exit 1; }
	clang-format -i $(SRC) $(TEST_SRC) $(shell find src tests -name '*.h')

dtb: dtb/rvm.dtb

dtb/rvm.dtb: dtb/rvm.dts
	dtc -I dts -O dtb -o $@ $<

coverage:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory test EXTRA_CFLAGS='--coverage -O0' EXTRA_LDFLAGS='--coverage'

clean:
	rm -rf $(BUILD) $(BIN) $(TEST_BIN) *.gcda *.gcno
	@echo "clean"

run: $(BIN)
	./$(BIN) -m 256 -k vmlinux -d disk.img --create-disk --stats

help:
	@echo "targets: all test lint format dtb coverage clean run lib"
	@echo "variables: MODE=release|debug|sanity  WERROR=1  TEST_ARGS=-v"

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)
