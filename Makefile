CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = tarsau

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.sau