noinst_LIBRARIES = libaice.a
libaice_a_SOURCES = aice_port.h \
		    aice_usb.c \
		    aice_usb.h \
		    config.h \
		    nds32_edm.h \
		    nds32_insn.h \
		    nds32_reg.c \
		    nds32_reg.h \
		    log.c \
		    log.h \
		    aice_usb_patch.h

libaice_a_CFLAGS = -Wall -Wextra -Wno-unused -D__LIBAICE__
if BUILD_MINGW_BINARY
libaice_a_CFLAGS += -mno-cygwin -DCYG
else
libaice_a_CFLAGS +=
endif
