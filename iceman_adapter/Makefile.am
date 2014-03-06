bin_PROGRAMS = ICEman
ICEman_SOURCES = iceman_adapter.c \
                 iceman_socket.c \
                 iceman_socket.h

openocd_cfgdir = $(bindir)
dist_openocd_cfg_DATA = openocd.cfg.tpl sw-reset-seq.tpl

openocd_boardcfgdir = $(bindir)/board
dist_openocd_boardcfg_DATA = board/nds32_xc5.cfg.tpl

openocd_interfacecfgdir = $(bindir)/interface
dist_openocd_interfacecfg_DATA = interface/nds32-aice.cfg.tpl

openocd_targetcfgdir = $(bindir)/target
dist_openocd_targetcfg_DATA = target/nds32.cfg.tpl

if USE_LIBUSB0
DRIVERLIBS = $(LIBUSB0_LIBS)
DRIVERCFLAGS = $(LIBUSB0_CFLAGS)
endif

if USE_LIBUSB1
DRIVERLIBS = $(LIBUSB1_LIBS)
DRIVERCFLAGS = -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
endif

ICEman_LDADD = ../aice/libaice.la $(DRIVERLIBS)
if BUILD_MINGW_BINARY
ICEman_LDADD += -lwsock32
else
ICEman_LDADD +=
endif


ICEman_CFLAGS = -I$(srcdir)/../aice -Wall -Wextra $(DRIVERCFLAGS)
if BUILD_MINGW_BINARY
ICEman_CFLAGS += -DCYG
else
ICEman_CFLAGS +=
endif

ICEman_LDFLAGS = -all-static
