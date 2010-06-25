#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([pam-phidgetrfid], [1.0.4], [enlightened.despot@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([pam-module/pam-phidgetrfid.c])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AM_PROG_CC_C_O

#check for PAM libraries and includes (with thanks to pam_krb5)
AC_SEARCH_LIBS([pam_set_data], [pam])
AC_CHECK_HEADERS([security/pam_modules.h], ,
    [AC_CHECK_HEADERS([pam/pam_modules.h], ,
        [AC_MSG_ERROR([No PAM header files found])])])
AC_CHECK_HEADERS([security/pam_ext.h], ,
    [AC_CHECK_HEADERS([pam/pam_ext.h])])
AC_CHECK_HEADERS([security/_pam_macros.h], ,
    [AC_CHECK_HEADERS([pam/_pam_macros.h])])
AC_CHECK_HEADERS([security/pam_misc.h], ,
    [AC_CHECK_HEADERS([pam/pam_misc.h])])

# Check for Phidget library
AC_CHECK_HEADERS([usb.h], , 
    [AC_MSG_ERROR([USB header file not found. Please verify the USB library is correctly installed.])])
AC_CHECK_HEADERS([phidget21.h], , 
    [AC_MSG_ERROR([Phidget header file not found. Please verify the Phidgets library is correctly installed.])])
AC_SEARCH_LIBS([CPhidget_open], [phidget21], ,
	[AC_MSG_ERROR([Phidget library not found. Please verify the Phidgets library is correctly installed.])])
 
# Check for header files.
AC_CHECK_HEADERS([stdlib.h string.h syslog.h])
AC_CHECK_HEADER(pthread.h, ,[AC_MSG_ERROR([pthreads header not found.])])

AC_CONFIG_FILES([Makefile
				common/Makefile
				pam-module/Makefile
				scan-tag/Makefile])
AC_OUTPUT
