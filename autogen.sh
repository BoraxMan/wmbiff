#!/bin/sh

# runs all the things necessary to rebuild files from CVS.
# no longer needed autoheader -l autoconf && \
# aclocal must be run before autoheader, so that 
# autoheader knows to create config.h, because automake
# complains if any macro other than its own specifies
# the file

# it's skanky that I have to look explicitly for a newer 
# version to invoke.

AUTORECONF=autoreconf

if [ -x /usr/bin/automake-1.7 ]; then 
    echo "Using automake 1.7";
    AUTOMAKE=automake-1.7
    ACLOCAL=aclocal-1.7
elif [ -x /usr/bin/automake-1.6 ]; then
    echo "Using automake 1.6";
    AUTOMAKE=automake-1.6
    ACLOCAL=aclocal-1.6
elif [ -x /usr/bin/automake-1.5 ]; then
    echo "Using automake 1.5";
    AUTOMAKE=automake-1.5
    ACLOCAL=aclocal-1.5
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
    if [ -x /usr/bin/autoconf-2.53 ]; then 
        echo "Using autoconf 2.53 (redhat-hack)";
        AUTOCONF=autoconf-2.53
        AUTOHEADER=autoheader-2.53
        AUTORECONF=autoreconf-2.53
    else
        AUTOCONF=autoconf
        AUTOHEADER=autoheader
    fi
fi

# something complains if ChangeLog does not exist
touch ChangeLog

if [ ! -e autoconf/libgnutls.m4 ] ; then
    echo "WARNING: autoconf/libgnutls.m4 does not exist.  autogen is unlikely to work."
fi
if [ ! -e autoconf/libgcrypt.m4 ] ; then
    echo "WARNING: autoconf/libgcrypt.m4 does not exist.  autogen is unlikely to work."
fi

if [ -h autoconf/config.guess ] && [ ! -e autoconf/config.guess ] ; then
    echo "autoconf/config.guess is a dangling symlink, removing."
    rm autoconf/config.guess
fi
if [ -h autoconf/config.sub ] && [ ! -e autoconf/config.sub ] ; then
    echo "autoconf/config.sub is a dangling symlink, removing."
    rm autoconf/config.sub
fi

ACLOCAL=${ACLOCAL} AUTOHEADER=${AUTOHEADER} \
AUTOCONF=${AUTOCONF} AUTOMAKE=${AUTOMAKE}  \
${AUTORECONF} --verbose --install --symlink && \
 ./configure --enable-dependency-tracking \
 make

#if [ -e /usr/share/aclocal/libgnutls.m4 ]; then
   #${ACLOCAL};
#else 
   #${ACLOCAL} -I autoconf;
#fi
# ${AUTOHEADER} && \
# ${AUTOMAKE} -a && \
# ${AUTOCONF} && \
# ./configure && \
# make

   # when adding gnome support, integrate:
   #aclocal -I /usr/share/aclocal/gnome-macros;
