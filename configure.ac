AC_INIT([openocd_adapter], [1.0], [hkwang@andestech.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_CC_C_O
AC_CHECK_LIB([usb], [usb_open], [USBLIB=-lusb])
AC_SUBST([USBLIB])

case $host in
  *-mingw*)
    is_mingw=yes
    ;;
  *)
    is_mingw=no
    ;;
esac

AM_CONDITIONAL([BUILD_MINGW_BINARY], [test $is_mingw = yes])

AC_CONFIG_FILES([Makefile aice_adapter/Makefile burner_adapter/Makefile iceman_adapter/Makefile])
AC_OUTPUT
