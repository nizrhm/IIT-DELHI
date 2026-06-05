CC = gcc

CFLAGS = -g -Wall -std=c99

TARGET = nizgdb

SRCS = debugger.c

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean