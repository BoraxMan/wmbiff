WMBIFF_VERSION="0.2q1"
DESTDIR=
prefix=/usr/local
bindir=${prefix}/bin
mandir=${prefix}/share/man
CONF=/etc
LIBDIR = -L/usr/X11R6/lib
LIBS   = -lXpm -lXext -lX11
CFLAGS = -O2 -Wall
#CFLAGS = -g -Wall -pedantic 

EXTRAFLAGS = -DWMBIFF_VERSION='$(WMBIFF_VERSION)'
#EXTRAFLAGS += -DDEBUG_POP3 -DDEBUG_IMAP4 -DDEBUG_LICQ \
#	-DDEBUG_MBOX -DDEBUG_MAILDIR -DDEBUG -DDEBUG_MAIL_COUNT

OBJS =	wmbiff.o socket.o \
	Pop3Client.o Imap4Client.o LicqClient.o mboxClient.o \
	maildirClient.o \
	../wmgeneral/wmgeneral.o \
	../wmgeneral/misc.o \
	../wmgeneral/list.o

INSTALL = /usr/bin/install
INSTALL_DIR	= $(INSTALL) -p -d -o root -g root -m 755
INSTALL_PROGRAM	= $(INSTALL) -p -o root -g root -m 755
INSTALL_FILE 	= $(INSTALL) -p -o root -g root -m 644


.c.o:
	gcc  $(CFLAGS) $(EXTRAFLAGS) -c $< -o $*.o

all: wmbiff-master.xpm wmbiff

wmbiff-master.xpm:
	ln -s wmbiff-master-led.xpm wmbiff-master.xpm

wmbiff:	$(OBJS)
	gcc $(CFLAGS) $(EXTRAFLAGS) -o wmbiff -lX11 -lnsl $^ $(LIBDIR) $(LIBS)

clean:
	for i in $(OBJS) ; do \
		rm -f $$i ; \
	done
	rm -f wmbiff *~ tags core

install: wmbiff
	$(INSTALL_DIR) $(DESTDIR)$(bindir)
	$(INSTALL_DIR) $(DESTDIR)$(mandir)/man1
	$(INSTALL_DIR) $(DESTDIR)$(mandir)/man5
	$(INSTALL_PROGRAM) wmbiff $(DESTDIR)$(bindir)
	$(INSTALL_FILE) wmbiff.1 $(DESTDIR)$(mandir)/man1
	$(INSTALL_FILE) wmbiffrc.5 $(DESTDIR)$(mandir)/man5

# CODING STYLE AND INDENTATION [2001-Mar-12]:
#
# There have been some problems with coding style in the past.  Many people
# contributed to wmbiff (thank you!), and the code got really messy.  To help
# resolve this, I used GNU indent with what I believe to be the most
# widely-accepted coding style options (K&R style) with 4-space TAB indents
# (because some of the code is highly nested) to clean up the code.  Not
# everyone may be happy with this, but has been determined to be necessary for
# consistency and legibility.
#
# In other words, make sure you run "make clean" or "make indent", and do not
# change the options on the indent command, before you submit patches against
# wmbiff.  This will make everyone's life easier.
#
#	-- Dwayne C. Litzenberger <dlitz@dlitz.net>
indent:
	indent -npro -kr -i4 -ts4 *.[ch] || true
