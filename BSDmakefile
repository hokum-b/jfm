CC = cc
CFLAGS = -Wall -g -I. -I/usr/local/include -I/usr/local/include/freetype2
CFLAGS += !=pkg-config --cflags x11 xft fontconfig freetype2 2>/dev/null
LDFLAGS = -L/usr/local/lib
LDFLAGS += !=pkg-config --libs x11 xft fontconfig freetype2 2>/dev/null
LDFLAGS += -lm

OBJS = jfm.o theme.o

all: jfm

jfm: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

jfm.o: jfm.c jfm.h
	$(CC) $(CFLAGS) -c jfm.c

theme.o: theme.c jfm.h
	$(CC) $(CFLAGS) -c theme.c

clean:
	rm -f jfm $(OBJS)

install: jfm
	install -m 755 jfm /usr/local/bin/jfm

uninstall:
	rm -f /usr/local/bin/jfm

.PHONY: all clean install uninstall
