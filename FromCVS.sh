#!/bin/sh

# runs all the things necessary to rebuild files from CVS.
# no longer needed autoheader -l autoconf && \
# aclocal must be run before autoheader, so that 
# autoheader knows to create config.h, because automake
# complains if any macro other than its own specifies
# the file

if [ -e /usr/share/aclocal/libgnutls.m4 ]; then
   aclocal;
else 
   aclocal -I autoconf;
fi
 autoheader && \
 automake -a && \
 autoconf && \
 ./configure && \
 make

   # when adding gnome support, integrate:
   #aclocal -I /usr/share/aclocal/gnome-macros;
