ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = $(LIBUSB1_CFLAGS)
AUTOMAKE_OPTIONS = subdir-objects
#SUBDIRS = helper \
#		  jtag/drivers 

bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = adapter.c \
					   aice_usb_command.c \
					   aice_usb_pack_format.c \
					   aice_print_info.c \
					   aice_log.c


if USE_LIBUSB0
DRIVERLIBS = $(LIBUSB0_LIBS)
DRIVERCFLAGS = $(LIBUSB0_CFLAGS)
aice_adapter_SOURCES += jtag/drivers/libusb0_common.c
endif

if USE_LIBUSB1
DRIVERLIBS = $(LIBUSB1_LIBS)
DRIVERCFLAGS = -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
aice_adapter_SOURCES += jtag/drivers/libusb1_common.c
endif

aice_adapter_LDFLAGS = $(DRIVERLIBS)

aice_adapter_CFLAGS = -I$(srcdir)/jtag/drivers \
					  -Wall $(DRIVERCFLAGS) \
					  -DHAVE_CONFIG_H

if BUILD_MINGW_BINARY
aice_adapter_CFLAGS += -DCYG -D__MINGW32__
else
aice_adapter_CFLAGS +=
endif


aice_adapter_LDFLAGS += -static 

PKGLIBS=libusb-1.0
aice_adapter_LDFLAGS += `pkg-config --libs $(PKGLIBS)`

if BUILD_MINGW_BINARY
aice_adapter_LDADD = -lws2_32 -lusb
else
if BUILD_CYGWIN_BINARY
aice_adapter_LDADD = -lusb-1.0 -ldl -lpthread
else
if BUILD_MAC_BINARY
aice_adapter_LDADD = -lusb-1.0 -ldl -lpthread
else
aice_adapter_LDADD = -lusb-1.0 -ldl -lpthread -lrt
endif
endif
endif

