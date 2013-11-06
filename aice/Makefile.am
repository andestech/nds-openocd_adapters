noinst_LTLIBRARIES = libaice.la

libaice_la_SOURCES = \
	aice_usb.c \
	nds32_reg.c \
	log.c \
	$(DRIVERFILES)

noinst_HEADERS = \
	aice_port.h \
	aice_usb.h \
	config.h \
	nds32_edm.h \
	nds32_insn.h \
	nds32_reg.h \
	aice_usb_helper.h \
	log.h \
	libusb0_common.h \
	libusb1_common.h \
	libusb_common.h

if USE_LIBUSB1
DRIVERFILES = libusb1_common.c
DRIVERCFLAGS = -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
endif

if USE_LIBUSB0
DRIVERFILES = libusb0_common.c
DRIVERCFLAGS = $(LIBUSB0_CFLAGS)
endif

libaice_la_CFLAGS = -std=gnu99 -Wall -Wextra -Wno-unused $(DRIVERCFLAGS)
if BUILD_MINGW_BINARY
libaice_la_CFLAGS += -DCYG
else
libaice_la_CFLAGS +=
endif

libaice_la_LDFLAGS = -static
