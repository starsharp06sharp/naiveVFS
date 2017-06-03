CFLAGS := -Wall -O2 -std=gnu99 $(shell pkg-config fuse --cflags) -Iheaders
LDFLAGS := $(shell pkg-config fuse --libs)

targets = naivefs

all: $(targets)

block.o: src/block.c headers/block.h
	$(CC) -c $< -o $@ $(CFLAGS)

file.o: src/file.c headers/file.h
	$(CC) -c $< -o $@ $(CFLAGS)

path.o: src/path.c headers/path.h
	$(CC) -c $< -o $@ $(CFLAGS)

main.o: src/main.c headers/base.h headers/block.h headers/file.h
	$(CC) -c $< -o $@ $(CFLAGS)

naivefs: block.o file.o path.o main.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o
	rm -r $(targets)

.PHONY: clean
