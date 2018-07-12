# --------------------------------------------------------------------------
#
# Project       SDK examples
#
# File          Makefile
#
# Author        Axel Wernesr
#
# --------------------------------------------------------------------------
# Changelog
#
#     2017-06-10  AWe   update setting for flash device COM8, DIO, 4096 KB (512 KB + 512 KB)
#     2017-06-09  AWe   used from the "Unofficial Development Kit for Espressif ESP8266"
#                         and adapt it for this project
#                         https://github.com/CHERTS/esp8266-devkit
#                         https://github.com/CHERTS/esp8266-devkit/tree/master/Espressif/examples/ESP8266
#
# --------------------------------------------------------------------------

# --------------------------------------------------------------------------
# some definitions for the default project

# name for the target project
TARGET      ?= appl

# COM port settings.
# COM port number and baud rate:
ESPBAUD     ?= 460800
ESPPORT     ?= COM8

# build directory
BUILD_DIR_BASE  ?= build/

# firmware directory
FW_DIR_BASE     ?= firmware/

SOURCE_DIR_BASE     ?= .

# --------------------------------------------------------------------------
# Base directory for the compiler
# XTENSA_TOOLS_ROOT ?= c:/Espressif/xtensa-lx106-elf/bin/
XTENSA_TOOLS_ROOT ?= C:/SysGCC/esp8266/bin/

# base directory of the ESP8266 SDK package, absolute
SDK_BASE    ?= c:/Espressif/ESP8266_NONOS_SDK/

# esptool path and port
ESPTOOL     ?= C:/Espressif/esptool/esptool.py

# --------------------------------------------------------------------------

# Boot mode:
# Valid values are none, old, new
# none - non use bootloader
# old  - boot_v1.1
# new  - boot_v1.2+
# BOOT = new
BOOT ?= none

# Choose binaries to generate
#  0 =  $(TARGET)_$(FW_FILE_1_ADDR).binm, $(TARGET)_$(FW_FILE_2_ADDR).bin
#       e.g.:  eagle.flash.bin+eagle.irom0text.bin,
#  1 =  user1.bin for ota
#  2 =  user2.bin for ota

# APP = 1
APP ?= 0

# Flash Frequency for ESP8266
# Clock frequency for SPI flash interactions.
# Valid values are 20, 26, 40, 80 (MHz).
# SPI_SPEED = 40
SPI_SPEED ?= 40

# Flash Mode for ESP8266
# These set Quad Flash I/O or Dual Flash I/O modes.
# Valid values are  QIO, QOUT, DIO, DOUT
# SPI_MODE = QIO
SPI_MODE ?= DIO

# Flash Size for ESP8266
# Size of the SPI flash, given in megabytes.
# ESP-12, ESP-12E and ESP-12F modules (and boards that use them such as NodeMCU, HUZZAH, etc.) usually
# have at least 4 megabyte / 4MB (sometimes labelled 32 megabit) flash.
# If using OTA, some additional sizes & layouts for OTA "firmware slots" are available.
# If not using OTA updates then you can ignore these extra sizes:
#
# Valid values vary by chip type: 1, 2, 3, 4, 5, 6, 7, 8, 9
#
# |SPI_SIZE_MAP|flash_size arg | Number of OTA slots | OTA Slot Size | Non-OTA Space |
# |------------|---------------|---------------------|---------------|---------------|
# |1           |256KB          | 1 (no OTA)          | 256KB         | N/A           |
# |0           |512KB          | 1 (no OTA)          | 512KB         | N/A           |
# |2           |1MB            | 2                   | 512KB         | 0KB           |
# |3           |2MB            | 2                   | 512KB         | 1024KB        |
# |4           |4MB            | 2                   | 512KB         | 3072KB        |
# |5           |2MB-c1         | 2                   | 1024KB        | 0KB           |
# |6           |4MB-c1         | 2                   | 1024KB        | 2048KB        |
# |8           |8MB [^]        | 2                   | 1024KB        | 6144KB        |
# |9           |16MB [^]       | 2                   | 1024KB        | 14336KB       |
#
# [^] Support for 8MB & 16MB flash size is not present in all ESP8266 SDKs. If your SDK doesn't support these flash sizes, use 4MB.
#

SPI_SIZE_MAP   ?= 0
SPI_FLASH_SIZE ?= 512KB
