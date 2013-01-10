bin_PROGRAMS = burner_adapter
burner_adapter_SOURCES = burner_action.c \
			 burner_action.h \
			 burner_server.c \
			 burner_socket.c \
			 burner_socket.h

if BUILD_MINGW_BINARY
burner_adapter_CFLAGS = -mno-cygwin -DCYG
else
burner_adapter_CFLAGS =
endif

if BUILD_MINGW_BINARY
burner_adapter_LDADD = -lwsock32
else
burner_adapter_LDADD =
endif

