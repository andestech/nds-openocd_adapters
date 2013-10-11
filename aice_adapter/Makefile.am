bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = adapter.c \
		       adapter.h

if USE_LIBUSB0
DRIVERLIBS = $(LIBUSB0_LIBS)
endif

if USE_LIBUSB1
DRIVERLIBS = $(LIBUSB1_LIBS)
endif

aice_adapter_LDADD = ../aice/libaice.a $(DRIVERLIBS)

aice_adapter_CFLAGS = -I$(srcdir)/../aice -Wall
if BUILD_MINGW_BINARY
aice_adapter_CFLAGS += -mno-cygwin -DCYG
else
aice_adapter_CFLAGS +=
endif
