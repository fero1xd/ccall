CC = clang
CFLAGS = -Wall -Wextra -Iinclude
TARGET = bin/main

all: build

build: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all build run clean
