# --------------------------------------------------------------------------
#
# Project       IoT - Internet of Things
#
# File          Makefile
#
# Author        Axel Werner
#
# --------------------------------------------------------------------------
# Changelog
#
#     2018-04-13  AWe   modified, so that flashing does not require a new link build
#     2017-08-17  AWe   fix SPI_SIZE_MAP table
#                       remove ESP_FLASH_SIZE_IX mapping
#     2017-06-09  AWe   use Makefile from the "Unofficial Development Kit for Espressif ESP8266"
#                         and adapt it for this project
#                         https://github.com/CHERTS/esp8266-devkit
#                         https://github.com/CHERTS/esp8266-devkit/tree/master/Espressif/examples/ESP8266
#
# --------------------------------------------------------------------------

# Directory the Makefile is in. Please don't include other Makefiles before this.
#- THISDIR  := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/
THISDIR  :=

# name for the target project
TARGET          = IoT-Wifi-Switch

BUILDDIR := F:/working/Build/Projects/Github/$(TARGET)

MAKEFILE_DIR    = $(THISDIR)makefiles/

# --------------------------------------------------------------------------
# some definitions for the current project


# COM port settings.
# COM port number and baud rate:
ESPBAUD		?= 460800
ESPPORT		?= COM8

# SOURCE_DIR_BASE    = ../source/
SOURCE_DIR_BASE      =

# build directory
BUILD_DIR_BASE  = $(BUILDDIR)/build/

# firmware directory
FW_DIR_BASE     = $(BUILDDIR)/firmware/

# --------------------------------------------------------------------------

DEBUG_MEMLEAK    ?= no
DEBUG_HEAP_INFO  ?= no

# --------------------------------------------------------------------------

# Main settings includes
include	$(MAKEFILE_DIR)settings.mk

# Flash Size for ESP8266
# Size of the SPI flash, given in megabytes.
# ESP-12, ESP-12E and ESP-12F modules (and boards that use them such as NodeMCU, HUZZAH, etc.) usually
# have at least 4 megabyte / 4MB (sometimes labelled 32 megabit) flash.
# If using OTA, some additional sizes & layouts for OTA "firmware slots" are available.
# If not using OTA updates then you can ignore these extra sizes:
#
# Valid values vary by chip type: 1, 2, 3, 4, 5, 6, 7, 8, 9
#
# |SPI_SIZE_MAP|flash_size arg|Number of OTA slots|OTA Slot Size|Non-OTA Space|
# +------------+--------------+-------------------+-------------+-------------+
# | 1          | 256KB        | 1 (no OTA)        |  256KB      |    N/A      |
# | 0          | 512KB        | 1 (no OTA)        |  512KB      |    N/A      |
# | 2          |   1MB        | 2                 |  512KB      |     0KB     |
# | 3          |   2MB        | 2                 |  512KB      |  1024KB     |
# | 4          |   4MB        | 2                 |  512KB      |  3072KB     |
# | 5          |   2MB-c1     | 2                 | 1024KB      |     0KB     |
# | 6          |   4MB-c1     | 2                 | 1024KB      |  2048KB     |
# | 8          |   8MB [^]    | 2                 | 1024KB      |  6144KB     |
# | 9          |  16MB [^]    | 2                 | 1024KB      | 14336KB     |
# +------------+--------------+-------------------+-------------+-------------+
#
# [^] Support for 8MB & 16MB flash size is not present in all ESP8266 SDKs. If your SDK doesn't support these flash sizes, use 4MB.
#

# --------------------------------------------------------------------------
# Individual project settings (Optional)

# TARGET_DEVICE ?= NodeMCU
  TARGET_DEVICE ?= NodeMCU-Modul
# TARGET_DEVICE  ?= ESP-201

DEBUG_USE_GDB ?= no

SERVERNAME_PREFIX=esp8266-httpd
TCP_MAX_CONNECTIONS   = 10
MQTT_MAX_CONNECTIONS  = 1
HTTPD_MAX_CONNECTIONS = 9  # TCP_MAX_CONNECTIONS - MQTT_MAX_CONNECTIONS

ifeq ("$(TARGET_DEVICE)","NodeMCU")
   SPI_FLASH_SIZE = 4MB-c1
else
   ifeq ("$(TARGET_DEVICE)","NodeMCU-Modul")
      SPI_FLASH_SIZE = 4MB-c1
   else
      ifeq ("$(TARGET_DEVICE)","ESP-201")
         SPI_FLASH_SIZE = 1MB
      else
         SPI_FLASH_SIZE ?= 512KB
         TARGET_DEVICE ?= undef
      endif
   endif
endif

# You can build this example in three ways:
# 'separate' - Separate espfs and binaries, no OTA upgrade
# 'combined' - Combined firmware blob, no OTA upgrade
# 'ota'      - Combined firmware blob with OTA upgrades.
# Please do a 'make clean' after changing this.

 OUTPUT_TYPE=separate
# OUTPUT_TYPE=combined
# OUTPUT_TYPE=ota

define maplookup
$(patsubst $(strip $(1)):%,%,$(filter $(strip $(1)):%,$(2)))
endef

# Input definitions
# BOOT, APP
# SPI_SPEED [20, 26, 40, 80 (MHz)], SPI_MODE [QIO, QOUT, DIO, DOUT]
# Aus der Variablen SPI_FLASH_SIZE [256KB, 512KB, 1MB, 2MB, 4MB, 2MB-c1, 4MB-c1, 8MB, 16MB]
#     ermitteln: ESP_SPI_FLASH_SIZE_K, SPI_SIZE_MAP

SPI_SIZE_MAP         = $(call maplookup,$(SPI_FLASH_SIZE),512KB:0 256KB:1 1MB:2 2MB-c1:5 4MB-c1:6 2MB:3 4MB:4 8MB:8 16MB:9)

ESP_SPI_FLASH_SIZE_K = $(call maplookup,$(SPI_FLASH_SIZE),512KB:512 256KB:256 1MB:1024 2MB-c1:2048 4MB-c1:4096 2MB:2048 4MB:4096 8MB:8196 16MB:16384)
ESP_FLASH_FREQ_DIV   = $(call maplookup,$(SPI_SPEED),40:0 26:1 20:2 80:15)
ESPTOOL_FREQ = $(call maplookup,$(ESP_FLASH_FREQ_DIV),0:40m 1:26m 2:20m 0xf:80m 15:80m)
ESPTOOL_MODE = $(call maplookup,$(SPI_MODE),QIO:qio QOUT:qout DIO:dio DOUT:dout)
ESPTOOL_SIZE = $(SPI_FLASH_SIZE)

# Include options and target specific to the OUTPUT_TYPE
ESPTOOL_OPTS = --port $(ESPPORT) --baud $(ESPBAUD)
ESPTOOL_FLASHDEF = --flash_freq $(ESPTOOL_FREQ) --flash_mode $(ESPTOOL_MODE) --flash_size $(ESPTOOL_SIZE)

BLANKPOS    = $$(printf "0x%X" $$(($(ESP_SPI_FLASH_SIZE_K)*1024-0x2000)))
INITDATAPOS = $$(printf "0x%X" $$(($(ESP_SPI_FLASH_SIZE_K)*1024-0x4000)))

ifeq ("$(OUTPUT_TYPE)","separate")
   # In case of separate ESPFS and binaries, set the pos and length of the ESPFS here.

   # for small flashes we have to reduce the space for the espfs, so that the program can fit into the flash
   ifeq ("$(SPI_FLASH_SIZE)","256KB")
      ESPFS_POS =
      ESPFS_SIZE = 0
      MAX_PROG_SIZE = $$(printf "0x%X" $$(($(INITDATAPOS)-0x10000 )))
   else
      ifeq ("$(SPI_FLASH_SIZE)","512KB")
         MAX_PROG_SIZE  = 0x54000
         ESPFS_POS  = $$(printf "0x%X" $$((0x10000+$(MAX_PROG_SIZE) )))
         ESPFS_SIZE = $$(printf "0x%X" $$(($(INITDATAPOS)-$(ESPFS_POS) )))
      else
        ESPFS_SIZE = 0x40000
        ESPFS_POS  = $$(printf "0x%X" $$(($(INITDATAPOS)-$(ESPFS_SIZE) )))
        MAX_PROG_SIZE = $$(printf "0x%X" $$(($(ESPFS_POS)-0x10000 )))
      endif
   endif
endif

ifeq ("$(OUTPUT_TYPE)","combined")
   # In case of combined ESPFS and binaries
   MAX_PROG_SIZE = $$(printf "0x%X" $$(($(INITDATAPOS)-0x10000 )))
endif

ifeq ("$(OUTPUT_TYPE)","ota")
   # In case of combined ESPFS and binaries for OTA
   MAX_PROG_SIZE = $$(printf "0x%X" $$(($(INITDATAPOS)-0x10000 )))
endif


# --------------------------------------------------------------------------
# Basic project settings

# which modules (subdirectories) of the project to include in compiling
MODULES	= user modules mqtt

# USER_LIBS defined to forche rebuild on changes
USER_LIBS += $(BUILD_DIR_BASE)libesphttpd/libesphttpd.a \
             $(BUILD_DIR_BASE)libesphttpd/libwebpages-espfs.a

INCDIR += include \
          libesphttpd/include

SDK_INCDIR += third_party/include \
              driver_lib/include

# EXTRA_INCDIR    += C:/Espressif/xtensa-lx106-elf/xtensa-lx106-elf/include
EXTRA_INCDIR    += C:/SysGCC/esp8266/xtensa-lx106-elf/xtensa-lx106-elf/include

# libraries used in this project, mainly provided by the SDK
LIBS	= phy pp net80211 lwip wpa crypto main ssl wps smartconfig c gcc

# Add in driver and esphttpd libs
LIBS += \
        driver \
        esphttpd

# additonal setting for the current project
LDFLAGS += -g -O2
CFLAGS  += \
           -DICACHE_FLASH \
           -DMQTT_DEBUG_ON \
           -DDEBUG_ON \
           -DSERVERNAME_PREFIX=$(SERVERNAME_PREFIX) \
           -DHTTPD_MAX_CONNECTIONS=$(HTTPD_MAX_CONNECTIONS) \
           -DTCP_MAX_CONNECTIONS=$(TCP_MAX_CONNECTIONS) \
           -DUSE_OPTIMIZE_PRINTF

LDFLAGS += \
           -L$(BUILD_DIR_BASE) \
           -L$(BUILD_DIR_BASE)libesphttpd/

ifeq ("$(TARGET_DEVICE)","NodeMCU")
   CFLAGS       += -DNODEMCU
else
   ifeq ("$(TARGET_DEVICE)","NodeMCU-Modul")
      CFLAGS       += -DNODEMCU_MODUL
   else
      ifeq ("$(TARGET_DEVICE)","ESP-201")
         CFLAGS       += -DESP201
      endif
   endif
endif

# --------------------------------------------------------------------------
# setup build time and buils number

# Name of text file containing build number.
BUILD_NUMBER_FILE=$(SOURCE_DIR_BASE)build-number.txt

EXTRA_LDFLAGS = $(BUILD_NUMBER_LDFLAGS)
COMPONENT_LINKER_DEPS += $(BUILD_NUMBER_FILE)

# --------------------------------------------------------------------------
# libesphttpd specific settings

USE_HEATSHRINK       ?= no    # yes results in: Malloc sendBuff failed!
USE_GZIP_COMPRESSION ?= no    # requires zlib
USE_UGLIFYJS         ?= no

USE_BACKLOG_SUPPORT  ?= yes
USE_SSL_SUPPORT      ?= no
USE_CORS_SUPPORT     ?= no
USE_SHUTDOWN_SUPPORT ?= no    # option in httpd_freertos.c
USE_SO_REUSEADD      ?= no    # option in httpd_freertos.c

ifeq ("$(USE_HEATSHRINK)","yes")
   CFLAGS       += -DESPFS_HEATSHRINK
endif

ifeq ("$(USE_GZIP_COMPRESSION)","yes")
   CFLAGS       += -DUSE_GZIP_COMPRESSION
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

# --------------------------------------------------------------------------
# debug settings

ifeq ("$(DEBUG_MEMLEAK)","yes")
    CFLAGS += -DMEMLEAK_DEBUG
endif

ifeq ("$(DEBUG_HEAP_INFO)","yes")
    CFLAGS += -DDBG_HEAP_INFO
endif

ifeq ("$(ESPFS_POS)","")
   # No hardcoded espfs position: link it in with the binaries.
   LIBS += webpages-espfs
else
   # Hardcoded espfs location: Pass espfs position to rest of code
   CFLAGS += -DESPFS_POS=$(ESPFS_POS) -DESPFS_SIZE=$(ESPFS_SIZE)
endif

ifeq ("$(OUTPUT_TYPE)","ota")
   CFLAGS += -DOTA_FLASH_SIZE_K=$(ESP_SPI_FLASH_SIZE_K)
endif

ifeq ("$(DEBUG_USE_GDB)", "yes")
  MODULES      += gdbstub
  INCDIR       += gdbstub
  EXTRA_INCDIR += C:/SysGCC/esp8266/HAL/SRC/include
  LDFLAGS      += -ggdb
endif

CFLAGS += -DINITDATAPOS=$(INITDATAPOS) -DBLANKPOS=$(BLANKPOS)

# --------------------------------------------------------------------------
# Define default target. If not defined here the one in the included Makefile is used as the default one.

# we build both files for the web pages, but only one of them is required
LIBESPHTTPD =  $(BUILD_DIR_BASE)libesphttpd/libesphttpd.a \
               $(BUILD_DIR_BASE)libesphttpd/libwebpages-espfs.a \
               $(BUILD_DIR_BASE)libesphttpd/webpages.espfs

PHONIES = $(LIBESPHTTPD)

# Additional (maybe generated) ld scripts to link in
EXTRA_LD_SCRIPTS = $(BUILD_DIR_BASE)ldscript_memspecific.ld IoT-Wifi-Switch.ld

# Root includes
include $(MAKEFILE_DIR)common_nonos.mk

# _LD_SCRIPT = $(addprefix -Tld/,$(LD_SCRIPT))

# --------------------------------------------------------------------------
#  The Build Targets
# --------------------------------------------------------------------------

# Include options and target specific to the OUTPUT_TYPE
include $(MAKEFILE_DIR)Makefile.$(OUTPUT_TYPE)

$(PHONIES):
#	$(Q) mkdir -p $(BUILD_DIR_BASE)libesphttpd
	$(Q) make -C libesphttpd BUILD_DIR=$(BUILD_DIR_BASE)libesphttpd/ EXT_INCDIR=../include \
	             DEBUG_USE_GDB=$(DEBUG_USE_GDB) \
	             DEBUG_MEMLEAK=$(DEBUG_MEMLEAK) \
	             DEBUG_HEAP_INFO=$(DEBUG_HEAP_INFO) \
	             SERVERNAME_PREFIX=$(SERVERNAME_PREFIX) \
	             HTTPD_MAX_CONNECTIONS=$(HTTPD_MAX_CONNECTIONS) \
	             USE_HEATSHRINK="$(USE_HEATSHRINK)" \
	             USE_GZIP_COMPRESSION="$(USE_GZIP_COMPRESSION)" \
	             USE_UGLIFYJS="$(USE_UGLIFYJS)" \
	             USE_BACKLOG_SUPPORT="$(USE_BACKLOG_SUPPORT)" \
	             USE_SSL_SUPPORT="$(USE_SSL_SUPPORT)" \
	             USE_CORS_SUPPORT="$(USE_CORS_SUPPORT)" \
	             USE_SHUTDOWN_SUPPORT="$(USE_SHUTDOWN_SUPPORT)" \
	             USE_SO_REUSEADD="$(USE_SO_REUSEADD)" \
	             $@

clean:
	$(Q) echo "Clean complete project ..."
#	$(Q) rm -f $(APP_AR)
#	$(Q) rm -f $(TARGET_OUT) $(TARGET_MAP)
#	$(Q) rm -rf $(BUILD_DIR)
	$(Q) rm -rf $(BUILD_DIR_BASE)
	$(Q) rm -rf $(FW_DIR_BASE)
#	$(Q) echo "Clean libesphttpd ..."
#	$(Q) make -C libesphttpd clean BUILD_DIR=$(BUILD_DIR_BASE)libesphttpd/

cleanhtml:
	$(Q) echo "Clean libesphttpd ..."
	$(Q) rm -f $(LIBESPHTTPD)

help:
	@echo "make checkdirs    "
	@echo "make check        Check program size"
	@echo "make clean        Remove all build output"
	@echo "make cleanhtml    "
	@echo "make clobber      "
	@echo "make flash        Flash app, bootloader, partition table to a chip"
	@echo "make flashboot    "
	@echo "make flashblank   Clear user data in highest 4K block"
	@echo "make flashinit    Flash init data default and blank data."
#	@echo "make erase_flash  Erase entire flash contents"
	@echo "make fp           Show flash parameter"
	@echo "make flashhtml    "
	@echo "make rebuild      "
	@echo "make showdirs     "

# Include build number rules.
include $(MAKEFILE_DIR)buildnumber.mk
