CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall -Wextra -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -fstack-protector-strong -pie -fPIE
LIBS = $(shell pkg-config --libs gtk+-3.0)

SOURCES = spotify-status.c mpris.c config.c progressbar.c

OUT = spotify-status

all:
	$(CC) $(SOURCES) -o $(OUT) $(CFLAGS) $(LIBS)

clean:
	rm -f $(OUT)