bin_PROGRAMS = ICEman
ICEman_SOURCES = iceman_adapter.c \
                 iceman_socket.c \
                 iceman_socket.h

openocd_cfgdir = $(bindir)
dist_openocd_cfg_DATA = openocd.cfg.v5 \
			openocd.cfg.tpl \
			sw-reset-seq.tpl \
			reset-hold-script.tpl \
			hw_reset_hold_script.tpl 
dist_openocd_cfg_DATA += ndsv5_tdesc_bitfield_64.xml \
			 ndsv5_tdesc_bitfield.xml \
			 nds32_tdesc_bitfield.xml
dist_openocd_cfg_DATA += algorithm.bin
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
			 nds_reg_write.tcl
dist_openocd_cfg_DATA += 70-ndsusb-v1.rules \
			 ftdi_script-v1.sh


openocd_boardcfgdir = $(bindir)/board
dist_openocd_boardcfg_DATA = board/nds32_xc5.cfg.tpl \
			     board/nds_v5.cfg.tpl

openocd_interfacecfgdir = $(bindir)/interface
dist_openocd_interfacecfg_DATA = interface/100ask-openjtag.cfg \
		interface/axm0432.cfg \
		interface/aice_sdp.cfg \
		interface/aice-mini-plus.cfg \
		interface/aice_mini_plus_ftdi.cfg \
		interface/calao-usb-a9260-c01.cfg \
		interface/calao-usb-a9260-c02.cfg \
		interface/cortino.cfg \
		interface/digilent-hs1.cfg \
		interface/digilent-hs2.cfg \
		interface/digilent_jtag_hs3.cfg \
		interface/digilent_jtag_smt2.cfg \
		interface/dlp-usb1232h.cfg \
		interface/dp_busblaster.cfg \
		interface/dp_busblaster_kt-link.cfg \
		interface/flossjtag.cfg \
		interface/flossjtag-noeeprom.cfg \
		interface/flyswatter2.cfg \
		interface/flyswatter.cfg \
		interface/gw16042.cfg \
		interface/hilscher_nxhx10_etm.cfg \
		interface/hilscher_nxhx500_etm.cfg \
		interface/hilscher_nxhx500_re.cfg \
		interface/hilscher_nxhx50_etm.cfg \
		interface/hilscher_nxhx50_re.cfg \
		interface/hitex_lpc1768stick.cfg \
		interface/hitex_str9-comstick.cfg \
		interface/icebear.cfg \
		interface/incircuit-icprog.cfg \
		interface/iotlab-usb.cfg \
		interface/jtagkey2.cfg \
		interface/jtagkey2p.cfg \
		interface/jtagkey.cfg \
		interface/jtag-lock-pick_tiny_2.cfg \
		interface/kt-link.cfg \
		interface/lisa-l.cfg \
		interface/luminary.cfg \
		interface/luminary-icdi.cfg \
		interface/luminary-lm3s811.cfg \
		interface/m53evk.cfg \
		interface/mbftdi.cfg \
		interface/minimodule.cfg \
		interface/minispartan6.cfg \
		interface/nds32-aice.cfg.tpl \
		interface/neodb.cfg \
		interface/ngxtech.cfg \
		interface/olimex-arm-jtag-swd.cfg \
		interface/olimex-arm-usb-ocd.cfg \
		interface/olimex-arm-usb-ocd-h.cfg \
		interface/olimex-arm-usb-tiny-h.cfg \
		interface/olimex.cfg \
		interface/olimex-jtag-tiny.cfg \
		interface/oocdlink.cfg \
		interface/opendous_ftdi.cfg \
		interface/openocd-usb.cfg \
		interface/openocd-usb-hs.cfg \
		interface/openrd.cfg \
		interface/pipistrello.cfg \
		interface/redbee-econotag.cfg \
		interface/redbee-usb.cfg \
		interface/rowley-cc-arm-swd.cfg \
		interface/sheevaplug.cfg \
		interface/signalyzer.cfg \
		interface/signalyzer-lite.cfg \
		interface/stm32-stick.cfg \
		interface/swd-resistor-hack.cfg \
		interface/ti-icdi.cfg \
		interface/tumpa.cfg \
		interface/tumpa-lite.cfg \
		interface/turtelizer2-revB.cfg \
		interface/turtelizer2-revC.cfg \
		interface/um232h.cfg \
		interface/vpaclink.cfg \
		interface/xds100v2.cfg \
		interface/xds100v3.cfg
 

openocd_targetcfgdir = $(bindir)/target
dist_openocd_targetcfg_DATA = target/nds32.cfg.tpl \
			      target/nds32_target_cfg.tpl \
			      target/user_target_cfg_table.txt

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
