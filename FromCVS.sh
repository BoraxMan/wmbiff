#!/bin/sh

# runs all the things necessary to rebuild files from CVS.
autoheader -l autoconf && \
 aclocal -I autoconf && \
 automake -a && \
 autoconf && \
 ./configure && \
 make
