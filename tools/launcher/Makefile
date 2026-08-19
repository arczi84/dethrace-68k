AMIGA_ROOT ?= /opt/amiga
CC = $(AMIGA_ROOT)/bin/m68k-amigaos-gcc
CFLAGS = -m68020 -O2 -Wall -Wextra -Wno-pointer-sign \
	-I$(AMIGA_ROOT)/m68k-amigaos/ndk-include \
	-I$(AMIGA_ROOT)/m68k-amigaos/include \
	-DAMIGA
LDFLAGS = -noixemul

TARGET = DethraceLauncher

all: $(TARGET)

$(TARGET): dethrace_launcher.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
