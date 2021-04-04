CC=clang-9 -Ofast
CFLAGS=-g -Wall -Wextra -pedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter
LDFLAGS=-g

# CC=gcc
# CFLAGS=-g -pg -Wall -Wextra -pedantic  -Wno-gnu-zero-variadic-macro-arguments
# LDFLAGS=-g -pg

RM=rm
LDLIBS=-ledit

# gprof src/main > x.txt

SRC_OBJECTS=

SRC_TARGETS=\
	src/main

.PHONY: all
all: $(SRC_TARGETS) $(TEST_TARGETS)

./src/main: $(SRC_OBJECTS) ./src/main.o
	$(CC) $(LDFLAGS) ./src/main.o $(SRC_OBJECTS) $(LDLIBS) -o ./src/main

%.o: %.c ./src/*.h ./test/*.h
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_INPUT) 

clean:
	rm -f src/*.o
	rm -f test/*.o
	rm -f src/main
	rm -f test/flag
