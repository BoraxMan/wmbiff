bin_PROGRAMS = wmbiff
wmbiff_SOURCES = wmbiff.c socket.c Pop3Client.c LicqClient.c mboxClient.c \
	maildirClient.c Imap4Client.c tlsComm.c tlsComm.h ShellClient.c  \
	passwordMgr.c passwordMgr.h charutil.c charutil.h Client.h
wmbiff_LDADD = -L../wmgeneral -lwmgeneral
wmbiff_DEPENDENCIES = ../wmgeneral/libwmgeneral.a
man_MANS = wmbiff.1 wmbiffrc.5
skindir = $(datadir)/wmbiff/skins
skin_DATA = wmbiff-master-led.xpm wmbiff-master-contrast.xpm

EXTRA_DIST = $(man_MANS) sample.wmbiffrc

MAINTAINERCLEANFILES = Makefile.in

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
# In other words, make sure you run "make clean" and "make indent", and do not
# change the options on the indent command, before you submit patches against
# wmbiff.  This will make everyone's life easier.
#
#	-- Dwayne C. Litzenberger <dlitz@dlitz.net>
indent:
	indent -npro -kr -i4 -ts4 $(wmbiff_SOURCES) || true

dist-hook-local: indent
dist-clean-local:
	test -L wmbiff-master.xpm && rm wmbiff-master.xpm
