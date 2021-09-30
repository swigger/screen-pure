CC=gcc
OBJS=acls.o ansi.o attacher.o braille.o braille_tsi.o canvas.o comm.o display.o encoding.o fileio.o help.o input.o kmapdef.o layer.o layout.o list_display.o list_generic.o list_window.o loadav.o logfile.o mark.o misc.o nethack.o process.o pty.o putenv.o resize.o sched.o screen.o search.o socket.o teln.o termcap.o term.o tty.o utmp.o viewport.o window.o

CFLAGS=-I. -DETCSCREENRC='"/usr/etc/screenrc"' -DSCREENENCODINGS='"/usr/share/screen/utf8encodings"' -DHAVE_CONFIG_H -DGIT_REV='""' -g -O0 -D_GNU_SOURCE
LDFLAGS=-lcurses  -lcrypt

all:screen

screen:$(OBJS)
	gcc -o $@ $(OBJS) $(LDFLAGS)

%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	-@ rm -f *.o screen
