bin_PROGRAMS = ICEman
ICEman_SOURCES = iceman_adapter.c

openocd_cfgdir = $(bindir)
dist_openocd_cfg_DATA = openocd.cfg.tpl sw-reset-seq.tpl

openocd_boardcfgdir = $(bindir)/board
dist_openocd_boardcfg_DATA = board/nds32_xc5.cfg.tpl

openocd_interfacecfgdir = $(bindir)/interface
dist_openocd_interfacecfg_DATA = interface/nds32-aice.cfg.tpl

openocd_targetcfgdir = $(bindir)/target
dist_openocd_targetcfg_DATA = target/nds32v2.cfg \
			      target/nds32v3.cfg \
			      target/nds32v3m.cfg

ICEman_LDADD = ../aice/libaice.a $(USBLIB)

ICEman_CFLAGS = -I$(srcdir)/../aice -Wall -Wextra
if BUILD_MINGW_BINARY
ICEman_CFLAGS += -mno-cygwin -DCYG
else
ICEman_CFLAGS +=
endif
