dnl Process this file with autoconf to produce a configure script.

dnl Initialize autoconf.
AC_PREREQ(2.68)
AC_INIT([PHF], [0.0], [ademakov@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([phf/builder.h])

dnl Initialize automake.
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

dnl Determine the host system type.
AC_CANONICAL_HOST

dnl check for API variants
AC_GNU_SOURCE

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_CXX_C_O
AM_PROG_AR
AC_PROG_RANLIB
AC_PROG_INSTALL

dnl Checks for libraries.

dnl Checks for header files.
#AC_CHECK_HEADERS([xmmintrin.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext], [mandatory])

dnl Checks for library functions.

dnl Check command line arguments

AC_CONFIG_FILES([
	Makefile
	phf/Makefile
	examples/Makefile
	examples/publicsuffix/Makefile])
AC_OUTPUT
