prefix=
BIN=/usr/X11R6/bin
CONF=/etc
LIBDIR = -L/usr/X11R6/lib
LIBS   = -lXpm -lXext -lX11
CFLAGS = -O2 -Wall
OBJS =	wmbiff.o Pop3Client.o \
	../wmgeneral/wmgeneral.o \
	../wmgeneral/misc.o \
	../wmgeneral/list.o


.c.o:
	cc -I/usr/X11R6/share/include $(CFLAGS) -c -Wall $< -o $*.o

wmbiff: $(OBJS)
	cc $(CFLAGS) -o wmbiff $^ $(LIBDIR) $(LIBS)
	strip wmbiff

all:: wmbiff

clean::
	for i in $(OBJS) ; do \
		rm -f $$i ; \
	done
	rm -f wmbiff *~ tags

install:: wmbiff
	cp -f wmbiff $(prefix)$(BIN)/
	chmod 755 $(prefix)$(BIN)/wmbiff
