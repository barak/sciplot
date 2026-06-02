# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([SciPlot], [1.37], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([SciPlot.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign -Wall])

# Checks for programs.
AM_PROG_AR
AC_PROG_CC
LT_INIT

# GTK4 and Cairo
PKG_CHECK_MODULES([GTK4], [gtk4 >= 4.0 cairo cairo-ps pangocairo])

AC_SEARCH_LIBS([hypot], [m])

# help2man for man page generation
AC_PATH_PROG([HELP2MAN], [help2man])
AM_CONDITIONAL([HAVE_HELP2MAN], [test -n "$HELP2MAN"])

# Epilogue
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
