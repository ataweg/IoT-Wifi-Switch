# Makefile, only used on esp8266 RTOS and non-RTOS SDK. ESP32 uses component.mk instead.

# Directory the Makefile is in. Please don't include other Makefiles before this.
THISDIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/

# Include httpd config from lower level, if it exists
-include ../esphttpdconfig.mk


# Default options. If you want to change them, please create ../esphttpdconfig.mk with the options you want in it.

HTMLDIR ?= ../html/

USE_HEATSHRINK       ?= no     # yes results in: Malloc sendBuff failed!
USE_GZIP_COMPRESSION ?= no     # requires zlib
USE_COMPRESS_W_YUI   ?= no
YUI-COMPRESSOR       ?= /usr/bin/yui-compressor
USE_UGLIFYJS         ?= no
JS_MINIFY_TOOL       ?= uglifyjs

USE_BACKLOG_SUPPORT  ?= no
USE_SSL_SUPPORT      ?= no
USE_CORS_SUPPORT     ?= no
USE_SHUTDOWN_SUPPORT ?= no
USE_SO_REUSEADD      ?= no

HTTPD_MAX_CONNECTIONS ?= 4

# For FreeRTOS
HTTPD_STACKSIZE ?= 2048

# Auto-detect ESP32 build if not given.
ifneq (,$(wildcard $(SDK_PATH)include/esp32))
   ESP32 ?= yes
   FREERTOS ?= yes
else
   ESP32 ?= no
   FREERTOS ?= no
endif

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_DIR      ?= build/

# Base directory for the compiler. Needs a / at the end; if not set it'll use the tools that are in
# the PATH.
XTENSA_TOOLS_ROOT ?=

# base directory of the ESP8266 SDK package, absolute
# Only used for the non-FreeRTOS build
SDK_BASE        ?= c:/Espressif/ESP8266_NONOS_SDK/

# Base directory of the ESP8266 FreeRTOS SDK package, absolute
# Only used for the FreeRTOS build
SDK_PATH        ?= c:/Espressif/ESP8266_RTOS_SDK/

# name for the target project
LIB             = $(BUILD_DIR)libesphttpd.a

# which modules (subdirectories) of the project to include in compiling
MODULES         = espfs core util
EXTRA_INCDIR  = $(EXT_INCDIR)
EXTRA_INCDIR += ./include \
                . \
                lib/heatshrink


# for non-os builds osapi.h includes "user_config.h" so we have to ensure that
# the include/libesphttpd folder is in the include path so this file can be found
ifeq ("$(FREERTOS)","no")
EXTRA_INCDIR    += ./include/libesphttpd
endif

# compiler flags using during compilation of source files
CFLAGS  = \
        -std=gnu99 \
        -Werror \
        -Wpointer-arith \
        -Wundef \
        -Wall \
        -Wl,-EL \
        -fno-inline-functions \
        -nostdlib \
        -mlongcalls \
        -mtext-section-literals  \
        -D__ets__ \
        -DICACHE_FLASH \
        -Wno-address \
        -DHTTPD_MAX_CONNECTIONS=$(HTTPD_MAX_CONNECTIONS) \
        -DHTTPD_STACKSIZE=$(HTTPD_STACKSIZE)

ifeq ("$(DEBUG_USE_GDB)", "yes")
CFLAGS  += \
           -Og \
           -ggdb
else
CFLAGS  += \
           -Os
endif

ifdef SERVERNAME_PREFIX
   CFLAGS += -DSERVERNAME_PREFIX='"$(SERVERNAME_PREFIX)"'
endif

# various paths from the SDK used in this project
SDK_LIBDIR      = lib/
SDK_LDDIR       = ld/

ifeq ("$(FREERTOS)","yes")
CFLAGS          += \
                -DFREERTOS \
                -DLWIP_OPEN_SRC \
                -ffunction-sections \
                -fdata-sections

ifeq ("$(ESP32)","yes")
SDK_INCDIR      = include \
                include/esp32 \
                driver_lib/include \
                extra_include \
                third_party/include \
                third_party/include/cjson \
                third_party/include/freertos \
                third_party/include/lwip \
                third_party/include/lwip/ipv4 \
                third_party/include/lwip/ipv6 \
                third_party/include/ssl

CFLAGS          += \
                -DESP32 \
                -DFREERTOS \
                -DLWIP_OPEN_SRC \
                -ffunction-sections \
                -fdata-sections
else
SDK_INCDIR      = include \
                include/freertos \
                include/espressif/esp8266 \
                include/espressif \
                extra_include \
                include/lwip \
                include/lwip/lwip \
                include/lwip/ipv4 \
                include/lwip/ipv6

CFLAGS          += \
                -DFREERTOS \
                -DLWIP_OPEN_SRC \
                -ffunction-sections \
                -fdata-sections
endif
   SDK_INCDIR      := $(addprefix -I$(SDK_PATH),$(SDK_INCDIR))
else
   SDK_INCDIR      = include
   SDK_INCDIR      := $(addprefix -I$(SDK_BASE),$(SDK_INCDIR))
endif


ifeq ("$(ESP32)","yes")
   TOOLPREFIX=xtensa-esp108-elf-
   CFLAGS+=-DESP32
else
   TOOLPREFIX=xtensa-lx106-elf-
endif

# select which tools to use as compiler, librarian and linker
CC      := $(XTENSA_TOOLS_ROOT)$(TOOLPREFIX)gcc
AR      := $(XTENSA_TOOLS_ROOT)$(TOOLPREFIX)ar
LD      := $(XTENSA_TOOLS_ROOT)$(TOOLPREFIX)gcc
OBJCOPY := $(XTENSA_TOOLS_ROOT)$(TOOLPREFIX)objcopy

MKESPFSIMAGE=$(BUILD_DIR)espfs/mkespfsimage/mkespfsimage.exe

####
#### no user configurable options below here
####
#SRC_DIR         := $(MODULES)
MODULE_BUILD_DIR := $(addprefix $(BUILD_DIR),$(MODULES) espfs/mkespfsimage/)

SRC             := $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.c))
OBJ             := $(patsubst %.c,$(BUILD_DIR)%.o,$(SRC))

MODULE_INCDIR   += $(addprefix -I,$(MODULES))
INCDIR          += $(addsuffix /include,$(MODULE_INCDIR))
EXTRA_INCDIR    := $(addprefix -I,$(EXTRA_INCDIR))

VERBOSE ?=
V ?= $(VERBOSE)
ifeq ("$(V)","1")
   Q :=
   vecho := @true
else
   Q := @
   vecho := @echo
endif

ifeq ("$(USE_HEATSHRINK)","yes")
   CFLAGS += -DESPFS_HEATSHRINK
endif

ifeq ("$(USE_GZIP_COMPRESSION)","yes")
   CFLAGS += -DUSE_GZIP_COMPRESSION
endif

ifeq ("$(USE_BACKLOG_SUPPORT)","yes")
   CFLAGS       += -DCONFIG_ESPHTTPD_BACKLOG_SUPPORT=1
endif

ifeq ("$(USE_SSL_SUPPORT)","yes")
   CFLAGS       += -DCONFIG_ESPHTTPD_SSL_SUPPORT=1
endif

ifeq ("$(USE_CORS_SUPPORT)","yes")
   CFLAGS       += -DCONFIG_ESPHTTPD_CORS_SUPPORT=1
endif

ifeq ("$(USE_SHUTDOWN_SUPPORT)","yes")
   CFLAGS       += -DCONFIG_ESPHTTPD_SHUTDOWN_SUPPORT=1
endif

ifeq ("$(USE_SO_REUSEADD)","yes")
   CFLAGS       += -DCONFIG_ESPHTTPD_SO_REUSEADDR=1
endif


ifeq ("$(ESP32)","yes")
   CFLAGS       += -DESP32=1
endif

CFLAGS          += \
                -DICACHE_FLASH       \
                -DUSE_OPTIMIZE_PRINTF

ifeq ("$(DEBUG_MEMLEAK)","yes")
    CFLAGS += -DMEMLEAK_DEBUG
endif

ifeq ("$(DEBUG_HEAP_INFO)","yes")
    CFLAGS += -DDBG_HEAP_INFO
endif

# --------------------------------------------------------------------------

vpath %.c $(MODULES)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC)  -MMD -MP $(MODULE_INCDIR) $(INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

# --------------------------------------------------------------------------

.PHONY: clean

all: checkdirs submodules $(LIB) $(BUILD_DIR)webpages.espfs $(BUILD_DIR)libwebpages-espfs.a $(MKESPFSIMAGE)

# Include all dependency files already generated
DEPS := $(OBJ:.o=.d)
-include $(DEPS)

$(LIB): $(MODULE_BUILD_DIR)  $(OBJ)
#	$(vecho) "AR $@"
#	$(vecho)  $(OBJ)
	$(Q) $(AR) cru $@ $(OBJ)

submodules: lib/heatshrink/Makefile

lib/heatshrink/Makefile:
	$(Q) echo "Heatshrink isn't found. Checking out submodules to fetch it."
	$(Q) git submodule init
	$(Q) git submodule update

checkdirs: $(MODULE_BUILD_DIR)

$(MODULE_BUILD_DIR):
	$(Q) mkdir -p $@

# ignore vim swap files
FIND_OPTIONS = -not -iname '*.swp' -not -iname '*.bak'

$(BUILD_DIR)webpages.espfs: $(HTMLDIR) $(HTMLDIR)/* $(MKESPFSIMAGE) $(MODULE_BUILD_DIR)
ifeq ("$(USE_COMPRESS_W_YUI)","yes")
	$(Q) echo "Build espfs file with web pages ..."
	$(Q) rm -rf $(BUILD_DIR)/html_compressed;
	$(Q) cp -r ../html $(BUILD_DIR)/html_compressed;
	$(Q) echo "Compression assets with yui-compressor. This may take a while..."
	$(Q) for file in `find $(BUILD_DIR)/html_compressed -type f -name "*.js"`; do $(YUI-COMPRESSOR) --type js $$file -o $$file; done
	$(Q) for file in `find $(BUILD_DIR)/html_compressed -type f -name "*.css"`; do $(YUI-COMPRESSOR) --type css $$file -o $$file; done
	$(Q) awk "BEGIN {printf \"YUI compression ratio was: %.2f%%\\n\", (`du -b -s $(BUILD_DIR)/html_compressed/ | sed 's/\([0-9]*\).*/\1/'`/`du -b -s ../html/ | sed 's/\([0-9]*\).*/\1/'`)*100}"
# mkespfsimage will compress html, css, svg and js files with gzip by default if enabled
# override with -g cmdline parameter
	$(Q) cd $(BUILD_DIR)/html_compressed; find . $(FIND_OPTIONS) | $(MKESPFSIMAGE) > $(BUILD_DIR)webpages.espfs; cd ..;
else
  ifeq ("$(USE_UGLIFYJS)","yes")
	$(Q) echo "Build espfs file with web pages using uglifyjs ..."
	$(Q) rm -rf $(BUILD_DIR)/html_compressed;
	$(Q) cp -r ../html $(BUILD_DIR)/html_compressed;
	$(Q) echo "Compressing javascript assets with uglifyjs"
	$(Q) for file in `find $(BUILD_DIR)/html_compressed -type f -name "*.js"`; do $(JS_MINIFY_TOOL) $$file -c -m -o $$file; done
	$(Q) awk "BEGIN {printf \" compression ratio was: %.2f%%\\n\", (`du -b -s $(BUILD_DIR)/html_compressed/ | sed 's/\([0-9]*\).*/\1/'`/`du -b -s $(HTMLDIR) | sed 's/\([0-9]*\).*/\1/'`)*100}"
	$(Q) cd $(BUILD_DIR)/html_compressed; find . $(FIND_OPTIONS) | $(MKESPFSIMAGE) > $(BUILD_DIR)/webpages.espfs; cd ..;
  else
    ifeq ("$(USE_HEATSHRINK)","y")
	$(Q) echo "Build espfs file with web pages using heatshrink ..."
    else
	$(Q) echo "Build espfs file with web pages ..."
    endif
	$(Q) cd ../html; find . $(FIND_OPTIONS) | $(MKESPFSIMAGE) > $(BUILD_DIR)webpages.espfs; cd ..
  endif
endif

$(BUILD_DIR)libwebpages-espfs.a: $(BUILD_DIR)webpages.espfs
	$(Q) echo "Build libwebpages-espfs.a ..."
	$(Q) $(OBJCOPY) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.irom0.literal \
		$(BUILD_DIR)webpages.espfs $(BUILD_DIR)webpages.espfs.o.tmp
	$(Q) $(LD) -nostdlib -Wl,-r $(BUILD_DIR)webpages.espfs.o.tmp -o $(BUILD_DIR)webpages.espfs.o -Wl,-T webpages.espfs.ld
	$(Q) $(AR) cru $@ $(BUILD_DIR)webpages.espfs.o

$(MKESPFSIMAGE): espfs/mkespfsimage/ $(MODULE_BUILD_DIR)
	$(Q) echo "Build mkespfsimage ..."
	$(Q) $(MAKE) -C espfs/mkespfsimage BUILD_DIR="$(BUILD_DIR)espfs/mkespfsimage/" \
		USE_HEATSHRINK="$(USE_HEATSHRINK)" \
		USE_GZIP_COMPRESSION="$(USE_GZIP_COMPRESSION)"

clean:
	$(Q) echo "Clean libesphttpd ..."
	$(Q) rm -f $(LIB)
#	$(Q) find $(BUILD_DIR) -type f | xargs rm -f
#	$(Q) rm -rf $(MODULE_BUILD_DIR)
	$(Q) make -C espfs/mkespfsimage/ clean BUILD_DIR=$(BUILD_DIR)espfs/mkespfsimage/
	$(Q) rm -f $(BUILD_DIR)webpages.espfs* $(BUILD_DIR)libwebpages-espfs.a
ifeq ("$(USE_COMPRESS_W_YUI)","yes")
	$(Q) rm -rf $(BUILD_DIR)/html_compressed
endif

$(foreach bdir,$(MODULE_BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
