# Makefile for wmbiff
#
# $Id: Makefile,v 1.21 2002/02/02 18:04:19 jordi Exp $

# Disable gnutls crypto?
#WITHOUT_CRYPTO=	1

# If using v0.2.x of gnutls change the variabel below to 2
GNUTLS_MINOR_VERSION=3

# Use external GNU regexp lib?
#EXT_GNU_REGEX_LIB=1

# Use debugging?
#DEBUG=		1

PREFIX?=	/usr/local
BINDIR?=	$(PREFIX)/bin
MANDIR?=	$(PREFIX)/share/man

CC?=		gcc
LIBDIR?=	-L/usr/X11R6/lib
LIBS=		-lXpm -lXext -lX11
CFLAGS?=	-O2 -Wall -Wpointer-arith

###########################################################
# Nothing below here should need to be changed
###########################################################

WMBIFF_VERSION=	"0.3.6"
EXTRAFLAGS=	-DWMBIFF_VERSION='$(WMBIFF_VERSION)' 

# Default linux owners
INSTALL_USER=	root
INSTALL_GROUP=	root

# Debugging
ifdef DEBUG
CFLAGS=		-g -Wall -Wpointer-arith -Wwrite-strings -pedantic
#-W -Wmissing-noreturn -Wtraditional
endif

# FreeBSD overrides
ifeq ($(OSTYPE),FreeBSD)
INSTALL_GROUP=	wheel
EXT_GNU_REGEX_LIB=1
CFLAGS+=	-I/usr/X11R6/include/
endif

# External GNU RegExp lib... needed on FreeBSD
ifdef EXT_GNU_REGEX_LIB
LIBS+=		-lgnuregex
CFLAGS+=	-DEXT_GNU_REGEX_LIB
endif

# Crypto support
ifndef WITHOUT_CRYPTO
EXTRAFLAGS+=	-DWITH_TLS -DWITH_GCRYPT
LIBS+=		-lgnutls -lgcrypt
endif
ifeq ($(GNUTLS_MINOR_VERSION),3)
EXTRAFLAGS+=	-DGNUTLS_VER=3 
endif

INSTALL?=	/usr/bin/install
INSTALL_DIR?=	$(INSTALL) -d -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 755
INSTALL_PRG?=	$(INSTALL) -p -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 755 -s
INSTALL_FILE?=	$(INSTALL) -p -o $(INSTALL_USER) -g $(INSTALL_GROUP) -m 644


# Misc debug stuff :
#EXTRAFLAGS+=	-DDEBUG_POP3 -DDEBUG_IMAP4 -DDEBUG_LICQ \
#	-DDEBUG_MBOX -DDEBUG_MAILDIR -DDEBUG -DDEBUG_MAIL_COUNT \
#	-DDEBUG_COMM

# For malloc debug :
#CFLAGS	+= -DUSE_DMALLOC
#LIBS	+= -ldmalloc


###########################################################
# Real make stuff begins :
###########################################################

OBJS =	wmbiff.o socket.o \
	Pop3Client.o LicqClient.o mboxClient.o \
	maildirClient.o Imap4Client.o tlsComm.o \
	../wmgeneral/wmgeneral.o \
	../wmgeneral/misc.o \
	../wmgeneral/list.o \
	charutil.o

all: wmbiff

Imap4Client.o: Imap4Client.c Client.h Makefile

wmbiff.o: wmbiff-master.xpm wmbiff.c

.c.o:
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $< -o $*.o

wmbiff-master.xpm:
	ln -s wmbiff-master-led.xpm wmbiff-master.xpm

wmbiff: $(OBJS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o wmbiff $^ $(LIBDIR) $(LIBS)

clean:
	for i in $(OBJS) ; do \
		rm -f $$i ; \
	done
	rm -f wmbiff *~ tags core wmbiff-master.xpm

distclean: clean

install: wmbiff
	$(INSTALL_DIR) $(BINDIR)
	$(INSTALL_DIR) $(MANDIR)/man1
	$(INSTALL_DIR) $(MANDIR)/man5
	$(INSTALL_PROGRAM) wmbiff $(BINDIR)
	$(INSTALL_FILE) wmbiff.1 $(MANDIR)/man1
	$(INSTALL_FILE) wmbiffrc.5 $(MANDIR)/man5

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

