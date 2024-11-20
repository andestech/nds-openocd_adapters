ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = $(LIBUSB1_CFLAGS)

bin_PROGRAMS = ICEman
ICEman_SOURCES = iceman_adapter.c \
                 iceman_socket.c \
                 iceman_socket.h


# For ice folder
openocd_cfgdir = $(bindir)
dist_openocd_cfg_DATA = openocd.cfg.v5 \
	openocd.cfg.rv32 \
	openocd.cfg.rv64 \
	openocd.cfg.tpl \
	sw-reset-seq.tpl \
	reset-hold-script.tpl \
	hw_reset_hold_script.tpl

dist_openocd_cfg_DATA += ndsv5_tdesc_bitfield_64.xml \
	ndsv5_tdesc_bitfield.xml \
	nds32_tdesc_bitfield.xml

dist_openocd_cfg_DATA += custom_lib.tcl \
	debug_diag.tcl \
	debug_util.tcl \
	dmi.tcl \
	example_custom.tcl \
	nds_custom_reset_func.tcl \
	nds_custom_reset.tcl \
	nds_debug_defines.tcl \
	nds_diagnosis_funcs.tcl \
	nds_diagnosis.tcl \
	nds_dmi_read-bak.tcl \
	nds_dmi_write-bak.tcl \
	nds_reg_read.tcl \
	nds_reg_write.tcl \
	nds32_user.cfg


# For driver
if BUILD_FOR_WINDOWS
install-data-hook:
	cp -r $(srcdir)/libusb-AICE-driver $(bindir)/libusb-AICE-driver

else
dist_openocd_cfg_DATA += 70-ndsusb-v1.rules \
	ICEman.sh \
	ftdi_script-v1.sh

install-data-hook:
	chmod +x $(DESTDIR)$(bindir)/ICEman.sh
	chmod +x $(DESTDIR)$(bindir)/ftdi_script-v1.sh

endif


# For board, interface, target folder
install-data-hook:
	cp -r $(srcdir)/board $(bindir)/board
	cp -r $(srcdir)/interface $(bindir)/interface
	cp -r $(srcdir)/target $(bindir)/target


ICEman_LDADD = $(LIBUSB1_LIBS)

ICEman_CFLAGS = -Wall -Wextra -fcommon -DHAVE_LIBUSB1 $(LIBUSB1_CFLAGS)
ICEman_CFLAGS += -DRELSTR=\"`$(top_srcdir)/guess-rev.sh $(top_srcdir)`\"
ICEman_CFLAGS += -DPKGBLDDATE=\"`date +%F-%R`\"

ICEman_LDFLAGS = ../../openocd/src/.libs/libopenocd.a ../../openocd/jimtcl/libjim.a `pkg-config --libs libusb-1.0`

if BUILD_MINGW_BINARY
ICEman_LDADD += -lwsock32
ICEman_CFLAGS += -DCYG
ICEman_LDFLAGS += -lws2_32
else
ICEman_LDADD += -ldl
ICEman_LDFLAGS += -lutil
endif



