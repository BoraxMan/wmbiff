#!/bin/sh

# runs all the things necessary to rebuild files from CVS.
autoheader -l autoconf && \
 aclocal && \
 automake -a && \
 autoconf && \
 ./configure && \
 make
