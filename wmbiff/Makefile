prefix=
BIN=/usr/X11R6/bin
LIBDIR = -L/usr/X11R6/lib
LIBS   = -lXpm -lXext -lX11
CFLAGS = -O2 -Wall
# Modified for Debian GNU/Linux
# Changed paths to make debian/rules happy
OBJS =	wmbiff/wmbiff.o wmbiff/Pop3Client.o \
	wmgeneral/wmgeneral.o \
	wmgeneral/misc.o \
	wmgeneral/list.o
#OBJS = wmbiff.o \
#	../wmgeneral/wmgeneral.o \
#	../wmgeneral/misc.o \
#	../wmgeneral/list.o


.c.o:
	cc -I/usr/X11R6/share/include $(CFLAGS) -c -Wall $< -o $*.o

# Modified for Debian GNU/Linux
wmbiff: $(OBJS)
	cc $(CFLAGS) -o wmbiff/wmbiff $^ $(LIBDIR) $(LIBS)
#	cc $(CFLAGS) -o wmbiff $^ $(LIBDIR) $(LIBS)
	strip wmbiff/wmbiff
#	strip wmbiff

all:: wmbiff

# Modified for Debian GNU/Linux
clean::
	for i in $(OBJS) ; do \
		rm -f $$i ; \
	done
	rm -f wmbiff/wmbiff *~ tags
#	rm -f wmbiff *~ tags

# Modified for Debian GNU/Linux
install:: wmbiff
	cp -f wmbiff/wmbiff $(prefix)$(BIN)/
#	cp -f wmbiff $(prefix)$(BIN)/
	chmod 755 $(prefix)$(BIN)/wmbiff
