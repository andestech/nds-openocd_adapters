#define jtag_libusb_device_handle usb_dev_handle
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

#define alive_sleep usleep
#define LOG_ERROR(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
