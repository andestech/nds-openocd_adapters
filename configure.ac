AC_INIT([openocd_adapter], [5.4.0])
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
	AC_MSG_ERROR([Can not find libusb1-0(>= 1.0.9)])
])

# Platform check
case $host in
	*-mingw*)
		AC_MSG_NOTICE([Build on Msys2])
		is_mingw=yes
		;;
	*-cygwin*)
		AC_MSG_NOTICE([Build on Cygwin])
		is_cygwin=yes
		;;
	*-darwin*)
		AC_MSG_NOTICE([Build on Darwin])
		is_mac=yes
		;;
	*-linux*)
		AC_MSG_NOTICE([Build on Linux])
		;;
	*)
		AC_MSG_ERROR([Unsupported platform: $host. This program only supports MinGW, Cygwin, or macOS.])
		is_mingw=no
		is_cygwin=no
		is_mac=no
		;;
esac

# Cross-compile check(only support linux+mingw)
if test "$build" != "$host"; then
	if echo "$host" | grep -q "mingw"; then
		AC_MSG_NOTICE([Cross-compiling for MinGW on Linux])
		is_mingw=yes
		is_cygwin=no
		is_mac=no
	fi
fi

AM_CONDITIONAL([BUILD_MINGW_BINARY], [test "$is_mingw" = "yes"])
AM_CONDITIONAL([BUILD_CYGWIN_BINARY], [test "$is_cygwin" = "yes"])
AM_CONDITIONAL([BUILD_MAC_BINARY], [test "$is_mac" = "yes"])

AC_MSG_NOTICE([PKG_CONFIG_LIBDIR = $PKG_CONFIG_LIBDIR])
AC_MSG_NOTICE([PKG_CONFIG_PATH   = $PKG_CONFIG_PATH])

AC_CONFIG_FILES([Makefile iceman_adapter/Makefile])
AC_OUTPUT
