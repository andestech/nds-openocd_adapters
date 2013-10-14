bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = adapter.c \
		       adapter.h

if USE_LIBUSB0
DRIVERLIBS = $(LIBUSB0_LIBS)
DRIVERCFLAGS = $(LIBUSB0_CFLAGS)
endif

if USE_LIBUSB1
DRIVERLIBS = $(LIBUSB1_LIBS)
DRIVERCFLAGS = -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
endif

aice_adapter_LDADD = ../aice/libaice.a $(DRIVERLIBS)

aice_adapter_CFLAGS = -I$(srcdir)/../aice -Wall $(DRIVERCFLAGS)
if BUILD_MINGW_BINARY
aice_adapter_CFLAGS += -DCYG
else
aice_adapter_CFLAGS +=
endif
