bin_PROGRAMS = aice_adapter
aice_adapter_SOURCES = adapter.c \
		       adapter.h

aice_adapter_LDADD = ../aice/libaice.a $(USBLIB)

aice_adapter_CFLAGS = -I$(srcdir)/../aice -Wall
if BUILD_MINGW_BINARY
aice_adapter_CFLAGS += -mno-cygwin -DCYG
else
aice_adapter_CFLAGS +=
endif
