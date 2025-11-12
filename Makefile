# -------- Configuration --------
CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -O2 -D_GNU_SOURCE
INCLUDE := -Iinclude
SRC_DIR := src/userspace
BUILD_DIR := build/userspace

# Default program (override with "make PROG=rt_sort" etc.)
PROG ?= rt_matrix

SRC  := $(SRC_DIR)/$(PROG).c
OUT  := $(BUILD_DIR)/$(PROG)

# -------- Rules --------
all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $< -lrt

run: $(OUT)
	sudo $(OUT)

clean:
	rm -f $(BUILD_DIR)/*

list:
	@echo "Available programs:"
	@ls $(SRC_DIR) | grep '\.c' | sed 's/\.c//'

# Example usage:
# make PROG=rt_sort
# make PROG=rt_matmul
# make run PROG=rt_matrix