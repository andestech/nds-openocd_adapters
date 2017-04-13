CC = gcc
CFLAGS  = -Wall -std=gnu99 -I. -I./jtag/drivers -DHAVE_CONFIG_H
LDFLAGS = 
EXECUTABLE = aice_adapter
SOURCES = adapter.c \
		  aice_print_info.c \
		  aice_usb_command.c \
		  aice_usb_pack_format.c \
		  aice_log.c
OBJECTS = $(SOURCES:.c=.o)

UNAME_S := $(shell uname)

ifeq ($(UNAME_S), Linux)
	CFLAGS  += -m32 -DHAVE_LIBUSB1 -I./libusb/include/libusb-1.0
	SOURCES += jtag/drivers/libusb1_common.c 
	LDFLAGS += -m32 -ldl -lpthread libusb/lib/libusb-1.0.a -lrt
else 
	CFLAGS  += -m32 -I./libusb/include
	SOURCES += jtag/drivers/libusb0_common.c 
	LDFLAGS += -m32 -ldl -lpthread libusb/lib/libusb.a 
endif


$(EXECUTABLE): $(SOURCES) $(OBJECTS) 
	$(CC) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE) 

