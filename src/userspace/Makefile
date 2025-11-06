# Makefile for rt_sort in src/userspace

CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2 -D_GNU_SOURCE -I../../include
LDFLAGS =
TARGET = rt_sort
SRCS = rt_sort.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
