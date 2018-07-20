#!/bin/bash
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

function func_yes {
	rmmod ftdi_sio 2> /dev/null

	if [ ! -f /etc/modprobe.d/blacklist-ftdi_sio ]; then
		echo 'blacklist ftdi_sio' >> /etc/modprobe.d/blacklist-ftdi_sio 2> /dev/null
	fi

	if [ ! -f /etc/modprobe.d/blacklist-ftdi_sio.conf ]; then
		echo 'blacklist ftdi_sio' >> /etc/modprobe.d/blacklist-ftdi_sio.conf 2> /dev/null
	fi
}

function func_no {
	echo "Please issue the following commands before executing ICEman: sudo rmmod ftdi_sio"
}

echo -e "The kernel module ftdi_sio will be removed and added to the modprob blacklist.\n\
This will stop the ftdi_sio module from being installed when the FTDI USB serial device converter is detected and\n\
ensure ICEman to execute correctly.\n\
Are you sure you want to continue? (Please enter a number)"
select yn in "Yes" "No"; do
	case $yn in
		Yes ) func_yes; break;;
		No  ) func_no;  break;;
		*   ) echo "Please type a number corresponding to your choice!"
	esac
done

echo "Done!"

