#include "config.h"

#include <usb.h>
#include <errno.h>

int libusb_open (uint16_t vid, uint16_t pid, struct usb_dev_handle **handle)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	struct usb_bus *busses;

	usb_init ();
	usb_find_busses ();
	usb_find_devices ();

	busses = usb_get_busses ();
	for (bus = busses ; bus ; bus = bus->next)
	{
		for (dev = bus->devices ; dev ; dev = dev->next)
		{
			if ((dev->descriptor.idVendor == vid) && (dev->descriptor.idProduct == pid))
			{
				*handle = usb_open (dev);
				if (*handle == NULL)
					return -errno;

				return 0;
			}
		}
	}

	return -ENODEV;
}

void libusb_close(usb_dev_handle *dev)
{
	/* Close device */
	usb_close(dev);
}

int libusb_get_endpoints(struct usb_device *udev,
		uint32_t *usb_read_ep,
		uint32_t *usb_write_ep)
{
	struct usb_interface *iface = udev->config->interface;
	struct usb_interface_descriptor *desc = iface->altsetting;
	int i;

	for (i = 0; i < desc->bNumEndpoints; i++) {
		uint8_t epnum = desc->endpoint[i].bEndpointAddress;
		bool is_input = epnum & 0x80;
		if (is_input)
			*usb_read_ep = epnum;
		else
			*usb_write_ep = epnum;
	}

	return 0;
}
