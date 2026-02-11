CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
LIBS = $(shell pkg-config --libs gtk+-3.0)

SOURCES = spotify-status.c mpris.c

OUT = spotify-status

all:
	$(CC) $(SOURCES) -o $(OUT) $(CFLAGS) $(LIBS)

clean:
	rm -f $(OUT)