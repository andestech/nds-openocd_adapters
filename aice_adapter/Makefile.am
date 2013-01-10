bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = adapter.c \
		       adapter.h \
		       aice_port.h \
		       aice_usb.c \
		       aice_usb.h \
		       config.h \
		       log.c \
		       log.h \
		       nds32_edm.h \
		       nds32_insn.h \
		       nds32_reg.c \
		       nds32_reg.h

aice_adapter_LDADD = $(USBLIB)

if BUILD_MINGW_BINARY
aice_adapter_CFLAGS = -mno-cygwin -DCYG
else
aice_adapter_CFLAGS =
endif
