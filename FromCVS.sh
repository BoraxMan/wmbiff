#!/bin/sh

# runs all the things necessary to rebuild files from CVS.
autoheader -l autoconf && \
if [ -e /usr/share/aclocal/libgnutls.m4 ]; then
   aclocal;
else 
   aclocal -I autoconf;
fi
 automake -a && \
 autoconf && \
 ./configure && \
 make

   # when adding gnome support, integrate:
   #aclocal -I /usr/share/aclocal/gnome-macros;
