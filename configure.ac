AC_INIT([openocd_adapter], [1.0], [hkwang@andestech.com])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_LANG_C
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AM_PROG_AR

LT_INIT

PKG_CHECK_MODULES([LIBUSB1], [libusb-1.0 >= 1.0.9], [use_libusb1=yes], [use_libusb1=no])
PKG_CHECK_MODULES([LIBUSB0], [libusb], [use_libusb0=yes], [use_libusb0=no])

if test "$use_libusb1" = "no" -a "$usb_libusb0" = "no"; then
    AC_MSG_ERROR([Can not find libusb1-0 or libusb])
fi

case $host in
  *-mingw*)
    is_mingw=yes
    ;;
  *)
    is_mingw=no
    ;;
esac

AM_CONDITIONAL([BUILD_MINGW_BINARY], [test $is_mingw = yes])
AM_CONDITIONAL([USE_LIBUSB1], [test "$use_libusb1" = "yes"])
AM_CONDITIONAL([USE_LIBUSB0], [test "$use_libusb1" = "no" -a "$use_libusb0" = "yes"])

# Define variables
AC_SUBST([BUILD_ID], [`date +%Y%m%d%H`])
AC_SUBST([BRANCH_NAME], [`cd ${srcdir} ; git branch | cut -d ' ' -f 2 | sed '/^$/d'`])
AC_SUBST([COMMIT_ID], [`cd ${srcdir} ; git log | head -n 1 | cut -d ' ' -f 2`])

AC_CONFIG_FILES([Makefile aice_adapter/Makefile iceman_adapter/Makefile])
AC_OUTPUT
