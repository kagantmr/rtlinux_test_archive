################################################################################
# Top-level Makefile — build userspace programs in-place
#
# This Makefile compiles every .c file found in src/userspace into an
# executable with the same basename (e.g. src/userspace/rt_sort.c ->
# src/userspace/rt_sort). It places no artifacts in the repository root so
# you can run `make` from the project root.
#
################################################################################

CC       := gcc
CFLAGS   := -std=gnu11 -Wall -Wextra -O2 -D_GNU_SOURCE -Iinclude
LDFLAGS  :=

USRSRC   := src/userspace
SRCS     := $(wildcard $(USRSRC)/*.c)
PROGS    := $(patsubst $(USRSRC)/%.c,$(USRSRC)/%,$(SRCS))

.PHONY: all userspace clean run run-all

all: userspace

userspace: $(PROGS)

$(USRSRC)/%: $(USRSRC)/%.c
	@echo "CC $< -> $@"
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# run a single program: make run PROGRAM=rt_sort
run: 
	@if [ -z "$(PROGRAM)" ]; then \
		echo "Usage: make run PROGRAM=<program_name>  (examples: PROGRAM=rt_sort)"; exit 1; \
	fi
	$(USRSRC)/$(PROGRAM)

# convenience: run everything (in sequence)
run-all: $(PROGS)
	@for p in $(PROGS); do echo "Running $$p"; ./$$p; done

clean:
	@echo "Cleaning userspace builds..."
	-rm -f $(PROGS)

################################################################################
#+ Notes
#+ - Real-time programs often require root or capabilities (CAP_SYS_NICE,
#+   CAP_IPC_LOCK). If you see permission errors when running, use sudo or set
#+   capabilities (e.g. sudo setcap 'cap_sys_nice,cap_ipc_lock+ep' ./src/userspace/rt_sort).
################################################################################

