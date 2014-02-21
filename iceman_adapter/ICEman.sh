#!/bin/sh
chmod +x ICEman openocd
if [ "$?" != "0" ]; then
	exit 1
fi
chown root ICEman openocd
if [ "$?" != "0" ]; then
	exit 2
fi
chgrp root ICEman openocd
if [ "$?" != "0" ]; then
	exit 3
fi
chmod +s ICEman openocd
if [ "$?" != "0" ]; then
	exit 4
fi
echo "Done!"
