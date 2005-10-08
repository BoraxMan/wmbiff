
# _AC_PATH_X_DIRECT_CORRECTED
# -----------------
# Internal subroutine of _AC_PATH_X.
# Set ac_x_includes and/or ac_x_libraries.
m4_define([_AC_PATH_X_DIRECT_CORRECTED],
[# Standard set of common directories for X headers.
# Check X11 before X11Rn because it is often a symlink to the current release.
ac_x_header_dirs='
/usr/X11/include
/usr/X11R6/include
/usr/X11R5/include
/usr/X11R4/include

/usr/include/X11
/usr/include/X11R6
/usr/include/X11R5
/usr/include/X11R4

/usr/local/X11/include
/usr/local/X11R6/include
/usr/local/X11R5/include
/usr/local/X11R4/include

/usr/local/include/X11
/usr/local/include/X11R6
/usr/local/include/X11R5
/usr/local/include/X11R4

/usr/X386/include
/usr/x386/include
/usr/XFree86/include/X11

/usr/include
/usr/local/include
/usr/unsupported/include
/usr/athena/include
/usr/local/x11r5/include
/usr/lpp/Xamples/include

/usr/openwin/include
/usr/openwin/share/include'

if test "$ac_x_includes" = no; then
  # Guess where to find include files, by looking for Xlib.h.
  # First, try using that file with no special directory specified.
  AC_PREPROC_IFELSE([AC_LANG_SOURCE([@%:@include <X11/Xlib.h>])],
[# We can compile using X headers with no special include directory.
ac_x_includes=],
[for ac_dir in $ac_x_header_dirs; do
  if test -r "$ac_dir/X11/Xlib.h"; then
    ac_x_includes=$ac_dir
    break
  fi
done])
fi # $ac_x_includes = no

if test "$ac_x_libraries" = no; then
  # Check for the libraries.
  # See if we find them without any special options.
  # Don't add to $LIBS permanently.
  ac_save_LIBS=$LIBS
  LIBS="-lX11 $LIBS"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([@%:@include <X11/Xlib.h>],
				  [XrmInitialize ()])],
		 [LIBS=$ac_save_LIBS
# We can link X programs with no special library path.
ac_x_libraries=],
		 [LIBS=$ac_save_LIBS
for ac_dir in `echo "$ac_x_includes $ac_x_header_dirs" | sed s/include/lib/g`
do
  # Don't even attempt the hair of trying to link an X program!
  for ac_extension in a so sl; do
    if test -r $ac_dir/libX11.$ac_extension; then
      ac_x_libraries=$ac_dir
      break 2
    fi
  done
done])
fi # $ac_x_libraries = no
])# _AC_PATH_X_DIRECT


# _AC_PATH_X_CORRECTED
# ----------
# Compute ac_cv_have_x.
AC_DEFUN([_AC_PATH_X_CORRECTED],
[AC_CACHE_VAL(ac_cv_have_x,
[# One or both of the vars are not set, and there is no cached value.
ac_x_includes=no ac_x_libraries=no
_AC_PATH_X_XMKMF
_AC_PATH_X_DIRECT_CORRECTED
if test "$ac_x_includes" = no || test "$ac_x_libraries" = no; then
  # Didn't find X anywhere.  Cache the known absence of X.
  ac_cv_have_x="have_x=no"
else
  # Record where we found X for the cache.
  ac_cv_have_x="have_x=yes \
		ac_x_includes=$ac_x_includes ac_x_libraries=$ac_x_libraries"
fi])dnl
])


# AC_PATH_X
# ---------
# If we find X, set shell vars x_includes and x_libraries to the
# paths, otherwise set no_x=yes.
# Uses ac_ vars as temps to allow command line to override cache and checks.
# --without-x overrides everything else, but does not touch the cache.
AN_HEADER([X11/Xlib.h],  [AC_PATH_X])
AC_DEFUN([AC_PATH_X_CORRECTED],
[dnl Document the X abnormal options inherited from history.
m4_divert_once([HELP_BEGIN], [
X features:
  --x-includes=DIR    X include files are in DIR
  --x-libraries=DIR   X library files are in DIR])dnl
AC_MSG_CHECKING([for X])

AC_ARG_WITH(x, [  --with-x                use the X Window System])
# $have_x is `yes', `no', `disabled', or empty when we do not yet know.
if test "x$with_x" = xno; then
  # The user explicitly disabled X.
  have_x=disabled
else
  if test "x$x_includes" != xNONE && test "x$x_libraries" != xNONE; then
    # Both variables are already set.
    have_x=yes
  else
    _AC_PATH_X_CORRECTED
  fi
  eval "$ac_cv_have_x"
fi # $with_x != no

if test "$have_x" != yes; then
  AC_MSG_RESULT([$have_x])
  no_x=yes
else
  # If each of the values was on the command line, it overrides each guess.
  test "x$x_includes" = xNONE && x_includes=$ac_x_includes
  test "x$x_libraries" = xNONE && x_libraries=$ac_x_libraries
  # Update the cache value to reflect the command line values.
  ac_cv_have_x="have_x=yes \
		ac_x_includes=$x_includes ac_x_libraries=$x_libraries"
  AC_MSG_RESULT([libraries $x_libraries, headers $x_includes])
fi
])# AC_PATH_X_CORRECTED



# AC_PATH_XTRA_CORRECTED
# ------------
# Find additional X libraries, magic flags, etc.
AC_DEFUN([AC_PATH_XTRA_CORRECTED],
[AC_REQUIRE([AC_PATH_X_CORRECTED])dnl
if test "$no_x" = yes; then
  # Not all programs may use this symbol, but it does not hurt to define it.
  AC_DEFINE([X_DISPLAY_MISSING], 1,
	    [Define to 1 if the X Window System is missing or not being used.])
  X_CFLAGS= X_PRE_LIBS= X_LIBS= X_EXTRA_LIBS=
else
  if test -n "$x_includes"; then
    X_CFLAGS="$X_CFLAGS -I$x_includes"
  fi

  # It would also be nice to do this for all -L options, not just this one.
  if test -n "$x_libraries"; then
    X_LIBS="$X_LIBS -L$x_libraries"
dnl FIXME: banish uname from this macro!
    # For Solaris; some versions of Sun CC require a space after -R and
    # others require no space.  Words are not sufficient . . . .
    case `(uname -sr) 2>/dev/null` in
    "SunOS 5"*)
      AC_MSG_CHECKING([whether -R must be followed by a space])
      ac_xsave_LIBS=$LIBS; LIBS="$LIBS -R$x_libraries"
      AC_LINK_IFELSE([AC_LANG_PROGRAM()], ac_R_nospace=yes, ac_R_nospace=no)
      if test $ac_R_nospace = yes; then
	AC_MSG_RESULT([no])
	X_LIBS="$X_LIBS -R$x_libraries"
      else
	LIBS="$ac_xsave_LIBS -R $x_libraries"
	AC_LINK_IFELSE([AC_LANG_PROGRAM()], ac_R_space=yes, ac_R_space=no)
	if test $ac_R_space = yes; then
	  AC_MSG_RESULT([yes])
	  X_LIBS="$X_LIBS -R $x_libraries"
	else
	  AC_MSG_RESULT([neither works])
	fi
      fi
      LIBS=$ac_xsave_LIBS
    esac
  fi

  # Check for system-dependent libraries X programs must link with.
  # Do this before checking for the system-independent R6 libraries
  # (-lICE), since we may need -lsocket or whatever for X linking.

  if test "$ISC" = yes; then
    X_EXTRA_LIBS="$X_EXTRA_LIBS -lnsl_s -linet"
  else
    # Martyn Johnson says this is needed for Ultrix, if the X
    # libraries were built with DECnet support.  And Karl Berry says
    # the Alpha needs dnet_stub (dnet does not exist).
    ac_xsave_LIBS="$LIBS"; LIBS="$LIBS $X_LIBS -lX11"
    AC_LINK_IFELSE([AC_LANG_CALL([], [XOpenDisplay])],
		   [],
    [AC_CHECK_LIB(dnet, dnet_ntoa, [X_EXTRA_LIBS="$X_EXTRA_LIBS -ldnet"])
    if test $ac_cv_lib_dnet_dnet_ntoa = no; then
      AC_CHECK_LIB(dnet_stub, dnet_ntoa,
	[X_EXTRA_LIBS="$X_EXTRA_LIBS -ldnet_stub"])
    fi])
    LIBS="$ac_xsave_LIBS"

    # msh@cis.ufl.edu says -lnsl (and -lsocket) are needed for his 386/AT,
    # to get the SysV transport functions.
    # Chad R. Larson says the Pyramis MIS-ES running DC/OSx (SVR4)
    # needs -lnsl.
    # The nsl library prevents programs from opening the X display
    # on Irix 5.2, according to T.E. Dickey.
    # The functions gethostbyname, getservbyname, and inet_addr are
    # in -lbsd on LynxOS 3.0.1/i386, according to Lars Hecking.
    AC_CHECK_FUNC(gethostbyname)
    if test $ac_cv_func_gethostbyname = no; then
      AC_CHECK_LIB(nsl, gethostbyname, X_EXTRA_LIBS="$X_EXTRA_LIBS -lnsl")
      if test $ac_cv_lib_nsl_gethostbyname = no; then
	AC_CHECK_LIB(bsd, gethostbyname, X_EXTRA_LIBS="$X_EXTRA_LIBS -lbsd")
      fi
    fi

    # lieder@skyler.mavd.honeywell.com says without -lsocket,
    # socket/setsockopt and other routines are undefined under SCO ODT
    # 2.0.  But -lsocket is broken on IRIX 5.2 (and is not necessary
    # on later versions), says Simon Leinen: it contains gethostby*
    # variants that don't use the name server (or something).  -lsocket
    # must be given before -lnsl if both are needed.  We assume that
    # if connect needs -lnsl, so does gethostbyname.
    AC_CHECK_FUNC(connect)
    if test $ac_cv_func_connect = no; then
      AC_CHECK_LIB(socket, connect, X_EXTRA_LIBS="-lsocket $X_EXTRA_LIBS", ,
	$X_EXTRA_LIBS)
    fi

    # Guillermo Gomez says -lposix is necessary on A/UX.
    AC_CHECK_FUNC(remove)
    if test $ac_cv_func_remove = no; then
      AC_CHECK_LIB(posix, remove, X_EXTRA_LIBS="$X_EXTRA_LIBS -lposix")
    fi

    # BSDI BSD/OS 2.1 needs -lipc for XOpenDisplay.
    AC_CHECK_FUNC(shmat)
    if test $ac_cv_func_shmat = no; then
      AC_CHECK_LIB(ipc, shmat, X_EXTRA_LIBS="$X_EXTRA_LIBS -lipc")
    fi
  fi

  # Check for libraries that X11R6 Xt/Xaw programs need.
  ac_save_LDFLAGS=$LDFLAGS
  test -n "$x_libraries" && LDFLAGS="$LDFLAGS -L$x_libraries"
  # SM needs ICE to (dynamically) link under SunOS 4.x (so we have to
  # check for ICE first), but we must link in the order -lSM -lICE or
  # we get undefined symbols.  So assume we have SM if we have ICE.
  # These have to be linked with before -lX11, unlike the other
  # libraries we check for below, so use a different variable.
  # John Interrante, Karl Berry
  AC_CHECK_LIB(ICE, IceConnectionNumber,
    [X_PRE_LIBS="$X_PRE_LIBS -lSM -lICE"], , $X_EXTRA_LIBS)
  LDFLAGS=$ac_save_LDFLAGS

fi
AC_SUBST(X_CFLAGS)dnl
AC_SUBST(X_PRE_LIBS)dnl
AC_SUBST(X_LIBS)dnl
AC_SUBST(X_EXTRA_LIBS)dnl
])# AC_PATH_XTRA_CORRECTED
