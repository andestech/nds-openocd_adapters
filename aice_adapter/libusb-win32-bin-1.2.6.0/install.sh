#!/bin/bash

if [ $# = "0" ]; then
	echo please specify prefix
	exit 0
fi

PREFIX=$1

sed -i '1d' libusb.pc
sed -i 1i"prefix=$PREFIX" libusb.pc
sed -i "s@^libdir=.*\$@libdir='$PREFIX/lib'@" libusb.la

install -m 644 -D lib/gcc/libusb.a $PREFIX/lib/libusb.a
install -m 644 -D include/lusb0_usb.h $PREFIX/include/usb.h
install -m 644 -D libusb.pc $PREFIX/lib/pkgconfig/libusb.pc
install -m 755 -D libusb.la $PREFIX/lib/libusb.la
