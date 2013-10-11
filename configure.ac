AC_INIT([openocd_adapter], [1.0], [hkwang@andestech.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG([0.23])

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


AC_CONFIG_FILES([Makefile aice/Makefile aice_adapter/Makefile burner_adapter/Makefile iceman_adapter/Makefile])
AC_OUTPUT
