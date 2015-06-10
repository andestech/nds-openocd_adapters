#!/bin/bash 

#Clean 
rm -rf libusb 

UNAMESTR=`uname`
BUILD_DIR=`readlink -f ./`

#Install libusb 
mkdir -p ./libusb
if [[ "$UNAMESTR" == 'Linux' ]]; then
    # build libusb-1.0
    cd ./libusb-1.0.18 
    ./configure --prefix=$BUILD_DIR/libusb CFLAGS='-m32 -g3 -O0' CXXFLAGS='-m32' LDFLAGS='-m32' --disable-shared --disable-udev --disable-timerfd
    make install -j8
    rm -rf $BUILD_DIR/libusb/libusb-config
else
    # build libusb-win32
    cd ./libusb-win32-bin-1.2.6.0 
    ./install.sh $BUILD_DIR/libusb
fi

