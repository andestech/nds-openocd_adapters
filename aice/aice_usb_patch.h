#ifndef __AICE_USB_PATCH_H__
#define __AICE_USB_PATCH_H__

#include "config.h"

#include <usb.h>
#include <unistd.h>
#include <errno.h>

#define jtag_libusb_device_handle usb_dev_handle

#ifdef __LIBAICE__
#define jtag_libusb_open  libusb_open
#define jtag_libusb_bulk_write usb_bulk_write
#define jtag_libusb_bulk_read usb_bulk_read
#define jtag_libusb_reset_device usb_reset
#define jtag_libusb_device usb_device
#define jtag_libusb_get_device usb_device
#define jtag_libusb_set_configuration usb_set_configuration
#define jtag_libusb_claim_interface usb_claim_interface
#define jtag_libusb_get_endpoints libusb_get_endpoints
#define jtag_libusb_close libusb_close

#define LOG_ERROR(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define DEBUG_JTAG_IO(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)

#define alive_sleep	usleep
#define keep_alive()	do {} while(0)

static int libusb_open (uint16_t vid, uint16_t pid, struct usb_dev_handle **handle)
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

static void libusb_close(usb_dev_handle *dev)
{
	/* Close device */
	usb_close(dev);
}

static int libusb_get_endpoints(struct usb_device *udev,
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

static int64_t timeval_ms()
{
	struct timeval now;
	int retval = gettimeofday(&now, NULL);
	if (retval < 0)
		return retval;
	return (int64_t)now.tv_sec * 1000 + now.tv_usec / 1000;
}
#endif

#ifdef _WIN32
static inline unsigned usleep(unsigned int usecs)
{
	Sleep((usecs/1000));
	return 0;
}
#endif

#endif /* __AICE_USB_PATCH_H__ */
