# This makefile is for ESP8266 projects using ESP8266_NONOS_SDK, ESP-IDF for ESP32 use a different one.

USE_GZIP_COMPRESSION ?= no
USE_HEATSHRINK ?= no

CC = gcc
LD = $(CC)

THISDIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

VERBOSE ?=
V ?= $(VERBOSE)
ifeq ("$(V)","1")
   Q :=
   vecho := @true
else
   Q := @
   vecho := @echo
endif

CFLAGS= -I$(THISDIR)../../lib/heatshrink \
        -I$(THISDIR)../../include \
        -I$(THISDIR)/.. \
        -std=gnu99

ifeq ("$(USE_GZIP_COMPRESSION)","yes")
   CFLAGS       += -DESPFS_GZIP
endif

ifeq ("$(USE_HEATSHRINK)","yes")
   CFLAGS       += -DESPFS_HEATSHRINK
endif

OBJS=main.o heatshrink_encoder.o
TARGET=mkespfsimage.exe

TARGET  := $(addprefix $(BUILD_DIR),$(TARGET))
OBJS    := $(addprefix $(BUILD_DIR),$(OBJS))

$(BUILD_DIR)%.o: $(THISDIR)/%.c
	$(Q) $(CC) $(CFLAGS) -c -o $@ $^


$(TARGET): $(OBJS)
ifeq ("$(USE_GZIP_COMPRESSION)","yes")
	$(Q) $(CC) -o $@ $^ -lz
else
	$(Q) $(CC) -o $@ $^
endif

clean:
	$(Q) rm -f $(TARGET) $(OBJS)
