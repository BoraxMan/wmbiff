#!/bin/sh
# a short script to make sure that wmbiff builds under various
# options.  this is not necessary for normal users, and is 
# intended as a last-minute sanity check before a release.

make indent

echo NO-CRYPTO BUILD
make clean
WITHOUT_CRYPTO=1 make 

echo NORMAL BUILD
make clean
make 

echo DEBUG BUILD
make clean
DEBUG=1 make 

# doesn't really work on non -bsd
if test `uname` != 'Linux'; then
 echo LIKE BSD 
 make clean
 EXT_GNU_REGEX_LIB=1 make
fi

# like I said... before a release.
make clean
