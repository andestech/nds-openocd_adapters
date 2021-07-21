AC_INIT([openocd_adapter], [5.0.0])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_LANG_C
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AM_PROG_AR
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG([0.23])

LT_INIT

PKG_CHECK_MODULES([LIBUSB1], [libusb-1.0 >= 1.0.9], [
	use_libusb1=yes
	LIBUSB1_CFLAGS=`echo $LIBUSB1_CFLAGS | sed 's/-I/-isystem /'`
], [
	use_libusb1=no
])
PKG_CHECK_MODULES([LIBUSB0], [libusb], [use_libusb0=yes], [use_libusb0=no])

for hidapi_lib in hidapi hidapi-hidraw hidapi-libusb; do
	PKG_CHECK_MODULES([HIDAPI],[$hidapi_lib],[
		use_hidapi=yes
		echo "use_hidapi=true"
		break
	],[
		use_hidapi=no
	])
done

if test "$use_libusb1" = "no" -a "$usb_libusb0" = "no"; then
    AC_MSG_ERROR([Can not find libusb1-0 or libusb])
fi

case $host in
	*-mingw*)
		echo "is_mingw=true"
		is_mingw=yes
		;;
	*)
		echo "is_mingw=false"
		is_mingw=no
		;;
esac

case `uname` in
	CYGWIN*)
		echo "is_cygwin=true"
		is_cygwin=yes
		;;
	Darwin*)
		echo "is_mac=true"
		is_mac=yes
		;;
	*)
		echo "is_cygwin=false"
		is_cygwin=no
		is_mac=no
		;;
esac

AM_CONDITIONAL([BUILD_MINGW_BINARY], [test $is_mingw = yes])
AM_CONDITIONAL([BUILD_CYGWIN_BINARY], [test "$is_cygwin" = "yes"])
AM_CONDITIONAL([BUILD_MAC_BINARY], [test "$is_mac" = "yes"])
AM_CONDITIONAL([USE_LIBUSB1], [test "$use_libusb1" = "yes"])
AM_CONDITIONAL([USE_LIBUSB0], [test "$use_libusb1" = "no" -a "$use_libusb0" = "yes"])
AM_CONDITIONAL([USE_HIDAPI], [test "$use_hidapi" = "yes"])

AC_MSG_NOTICE([PKG_CONFIG_LIBDIR = $PKG_CONFIG_LIBDIR])
AC_MSG_NOTICE([PKG_CONFIG_PATH   = $PKG_CONFIG_PATH])

# Define variables
#AC_SUBST([BUILD_ID], [`date +%Y%m%d%H`])
#AC_SUBST([COMMIT_ID], [`cd ${srcdir} ; git log | head -n 1 | cut -d ' ' -f 2`])

AC_CONFIG_FILES([Makefile iceman_adapter/Makefile])
AC_OUTPUT
