CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g -pthread -Iinclude

TARGET = server
SRC = src/main.c src/server.c src/http.c src/thread_pool.c

all: $(TARGET)

$(TARGET): $(SRC) include/server.h include/http.h include/thread_pool.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean