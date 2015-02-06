AUTOMAKE_OPTIONS = subdir-objects
#SUBDIRS = helper \
#		  jtag/drivers 

bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = adapter.c \
					   aice_usb_command.c \
					   aice_usb_pack_format.c \
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

aice_adapter_CFLAGS = -I$(srcdir)/jtag/drivers \
					  -Wall $(DRIVERCFLAGS) \
					  -DHAVE_CONFIG_H

if BUILD_MINGW_BINARY
aice_adapter_CFLAGS += -DCYG -D__MINGW32__
else
aice_adapter_CFLAGS +=
endif


aice_adapter_LDFLAGS = -all-static 

if BUILD_MINGW_BINARY
aice_adapter_LDFLAGS += -lws2_32 -lusb
else
if BUILD_CYGWIN_BINARY
aice_adapter_LDFLAGS += -ldl -lpthread -lusb 
else
aice_adapter_LDFLAGS += -ldl -lpthread -lrt -lusb-1.0
endif
endif 

