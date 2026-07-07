CC = cc
CFLAGS = -Wall -g -I. $(shell pkg-config --cflags x11 xft fontconfig freetype2)
LDFLAGS = $(shell pkg-config --libs x11 xft fontconfig freetype2) -lm

all: jfm

jfm: jfm.o theme.o
	$(CC) -o $@ jfm.o theme.o $(LDFLAGS)

jfm.o: jfm.c jfm.h
	$(CC) $(CFLAGS) -c jfm.c

theme.o: theme.c jfm.h
	$(CC) $(CFLAGS) -c theme.c

clean:
	rm -f jfm *.o
