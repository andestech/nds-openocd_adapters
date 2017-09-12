bin_PROGRAMS = ICEman
ICEman_SOURCES = iceman_adapter.c \
                 iceman_socket.c \
                 iceman_socket.h

openocd_cfgdir = $(bindir)
dist_openocd_cfg_DATA = openocd.cfg.v5 openocd.cfg.tpl sw-reset-seq.tpl reset-hold-script.tpl nds32_tdesc_bitfield.xml ndsv5_tdesc_bitfield.xml
dist_openocd_cfg_DATA += ndsv5_tdesc_bitfield_64.xml

openocd_boardcfgdir = $(bindir)/board
dist_openocd_boardcfg_DATA = board/nds32_xc5.cfg.tpl

openocd_interfacecfgdir = $(bindir)/interface
dist_openocd_interfacecfg_DATA = interface/nds32-aice.cfg.tpl interface/olimex-arm-usb-tiny-h.cfg

openocd_targetcfgdir = $(bindir)/target
dist_openocd_targetcfg_DATA = target/nds32.cfg.tpl

if BUILD_CYGWIN_BINARY
DRIVERLIBS = $(LIBUSB0_LIBS) $(LIBUSB1_LIBS) 
DRIVERCFLAGS = $(LIBUSB0_CFLAGS) $(LIBUSB1_CFLAGS) 
else
DRIVERLIBS = $(LIBUSB1_LIBS)
DRIVERCFLAGS = -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
endif

ICEman_LDADD = $(DRIVERLIBS)
if BUILD_MINGW_BINARY
ICEman_LDADD += -lwsock32
else
ICEman_LDADD += -ldl
endif


ICEman_CFLAGS = -Wall -Wextra $(DRIVERCFLAGS)
# ICEman_CFLAGS += -I$(srcdir)/../../openocd-0.8.0/src -I$(srcdir)/../../openocd-0.8.0/src/target
if BUILD_MINGW_BINARY
ICEman_CFLAGS += -DCYG
else
ICEman_CFLAGS +=
endif

ICEman_CFLAGS += -DBUILD_ID=\"$(BUILD_ID)\" \
	-DBRANCH_NAME=\"$(BRANCH_NAME)\" \
	-DCOMMIT_ID=\"$(COMMIT_ID)\"

#ICEman_LDFLAGS = -all-static
#ICEman_LDFLAGS += ../../openocd/src/.libs/libopenocd.a ../../openocd/jimtcl/libjim.a

ICEman_LDFLAGS = ../../openocd/src/.libs/libopenocd.a ../../openocd/jimtcl/libjim.a
