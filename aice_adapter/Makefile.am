bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = aice_log.h \
					   aice_log.c \
					   adapter.h \
					   adapter.c

if USE_LIBUSB0
DRIVERLIBS = $(LIBUSB0_LIBS)
DRIVERCFLAGS = $(LIBUSB0_CFLAGS)
endif

if USE_LIBUSB1
DRIVERLIBS = $(LIBUSB1_LIBS)
DRIVERCFLAGS = -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
endif

aice_adapter_CFLAGS = -I$(srcdir)/../../openocd-0.8.0/src \
					  -I$(srcdir)/../../openocd-0.8.0/src/helper \
  					  -I$(srcdir)/../../openocd-0.8.0/src/jtag/aice \
  					  -Wall $(DRIVERCFLAGS)

if BUILD_MINGW_BINARY
aice_adapter_CFLAGS += -DCYG -D__MINGW32__
else
aice_adapter_CFLAGS +=
endif

aice_adapter_LDFLAGS = $(bindir)/../../build/openocd/src/.libs/libopenocd.a \
				   	   $(bindir)/../../build/openocd/jimtcl/libjim.a
if BUILD_MINGW_BINARY
aice_adapter_LDFLAGS += $(bindir)/../lib/libusb.a -lws2_32 -all-static
else
if BUILD_CYGWIN_BINARY
aice_adapter_LDFLAGS += $(bindir)/../lib/libusb.a -ldl -lpthread -all-static
else
aice_adapter_LDFLAGS += $(bindir)/../lib/libusb-1.0.a -ldl -lpthread -lrt -all-static 
endif
endif 

