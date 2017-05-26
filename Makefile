CFLAGS := -Wall -O2 -std=gnu99 $(shell pkg-config fuse --cflags) -Iheaders
LDFLAGS := $(shell pkg-config fuse --libs)

targets = expfs

all: $(targets)

block.o: src/block.c headers/block.h
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o
	rm -r $(targets)

.PHONY: clean
