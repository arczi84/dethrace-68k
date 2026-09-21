CROSS ?= /opt/amiga/bin/m68k-amigaos-
CC = $(CROSS)gcc
AR = $(CROSS)ar
CFLAGS ?= -O2 -m68060 -mhard-float -fno-baserel -noixemul
CPPFLAGS += -Iinclude
OBJECTS = client/minigl_base.o client/minigl_open.o client/minigl_dispatch_stub_gcc.o

all: lib/libminigl_dispatch.a

client/%.o: client/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

client/%.o: client/%.s
	$(CC) $(CFLAGS) -c $< -o $@

lib/libminigl_dispatch.a: $(OBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS)

.PHONY: all clean
