#!/bin/sh

# runs all the things necessary to rebuild files from CVS.
# no longer needed autoheader -l autoconf && \
# aclocal must be run before autoheader, so that 
# autoheader knows to create config.h, because automake
# complains if any macro other than its own specifies
# the file

# it's skanky that I have to look explicitly for a newer 
# version to invoke.

if [ -x /usr/bin/automake-1.6 ]; then 
    echo "Using automake 1.6";
  AUTOMAKE=automake-1.6
  ACLOCAL=aclocal-1.6
else
  AUTOMAKE=automake
  ACLOCAL=aclocal
fi

# debian, at least, handles this one correctly;
# this hack is for potential redhatters.
if [ -x /usr/bin/autoconf2.50 ]; then 
    echo "Using autoconf 2.5x";
  AUTOCONF=autoconf2.50
  AUTOHEADER=autoheader2.50
else
  AUTOCONF=autoconf
  AUTOHEADER=autoheader
fi

if [ -e /usr/share/aclocal/libgnutls.m4 ]; then
   ${ACLOCAL};
else 
   ${ACLOCAL} -I autoconf;
fi
 ${AUTOHEADER} && \
 ${AUTOMAKE} -a && \
 ${AUTOCONF} && \
 ./configure && \
 make

   # when adding gnome support, integrate:
   #aclocal -I /usr/share/aclocal/gnome-macros;
