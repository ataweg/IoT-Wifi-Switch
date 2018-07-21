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
#     2017-08-17  AWe   remove INCDIR
#                       modify CFLAGS
#                       add $(PHONIES) to PHONY:
#                       remove clean: target, this will done in the calling Makefile
#                       replace -p $(ESPPORT) -b $(ESPBAUD) with $(ESPTOOL_OPTS)
#                       replace -ff --> --flash_freq, -fm -> --flash_mode, -fs --> --flash_size
#     2017-07-06  AWe   add support for USER_LIBS
#     2017-06-09  AWe   use Makefile from the SDK and apdapt it for this project
#                       create a linker map file and dump the size
#
# --------------------------------------------------------------------------

#############################################################
#
# Root Level Makefile
#
# Version 2.0
#
# (c) by CHERTS <sleuthhound@gmail.com>
#
#############################################################

ifeq ($(BOOT), new)
    boot = new
else
    ifeq ($(BOOT), old)
        boot = old
    else
        boot = none
    endif
endif

ifeq ($(APP), 1)
    app = 1
else
    ifeq ($(APP), 2)
        app = 2
    else
        app = 0
    endif
endif

ifeq ($(SPI_SPEED), 26)
    freqdiv = 1
    flashimageoptions = --flash_freq 26m
else
    ifeq ($(SPI_SPEED), 20)
        freqdiv = 2
        flashimageoptions = --flash_freq 20m
    else
        ifeq ($(SPI_SPEED), 80)
            freqdiv = 15
            flashimageoptions = --flash_freq 80m
        else
            freqdiv = 0
            flashimageoptions = --flash_freq 40m
        endif
    endif
endif

ifeq ($(SPI_MODE), QOUT)
    mode = 1
    flashimageoptions += --flash_mode qout
else
    ifeq ($(SPI_MODE), DIO)
        mode = 2
        flashimageoptions += --flash_mode dio
    else
        ifeq ($(SPI_MODE), DOUT)
            mode = 3
            flashimageoptions += --flash_mode dout
        else
            mode = 0
            flashimageoptions += --flash_mode qio
        endif
    endif
endif

addr_upgrade = 0x01000

# |SPI_SIZE_MAP|size_map| flash |flash_size|addr_upgrade|
# +------------+--------+-------+----------+------------+
# | 1          | 1      |   256 | 256KB    |  0x81000   |
# | 2          | 2      |  1024 |   1MB    |  0x81000   |
# | 3          | 3      |  2048 |   2MB    |  0x81000   |
# | 4          | 4      |  4096 |   4MB    | 0x101000   |
# | 5          | 5      |  2048 |   2MB-c1 | 0x101000   |
# | 6          | 6      |  4096 |   4MB-c1 | 0x101000   |
# | 8          | 8      |  8192 |   8MB    | 0x101000   |
# | 9          | 9      | 16384 |  16MB    |  0x41000   |
# | 0          | 0      |   512 | 512KB    |  0x01000   |
# +------------+--------+-------+----------+------------+

ifeq ($(SPI_SIZE_MAP), 1)
  size_map = 1
  flash = 256
  flashimageoptions += --flash_size 256KB
else
  ifeq ($(SPI_SIZE_MAP), 2)
    size_map = 2
    flash = 1024
    flashimageoptions += --flash_size 1MB
    ifeq ($(app), 2)
      addr_upgrade = 0x81000
    endif
  else
    ifeq ($(SPI_SIZE_MAP), 3)
      size_map = 3
      flash = 2048
      flashimageoptions += --flash_size 2MB
      ifeq ($(app), 2)
        addr_upgrade = 0x81000
      endif
    else
      ifeq ($(SPI_SIZE_MAP), 4)
        size_map = 4
        flash = 4096
        flashimageoptions += --flash_size 4MB
        ifeq ($(app), 2)
          addr_upgrade = 0x81000
        endif
      else
        ifeq ($(SPI_SIZE_MAP), 5)
          size_map = 5
          flash = 2048
          flashimageoptions += --flash_size 2MB-c1
          ifeq ($(app), 2)
            addr_upgrade = 0x101000
          endif
        else
          ifeq ($(SPI_SIZE_MAP), 6)
            size_map = 6
            flash = 4096
            flashimageoptions += --flash_size 4MB-c1
            ifeq ($(app), 2)
              addr_upgrade = 0x101000
            endif
          else
            ifeq ($(SPI_SIZE_MAP), 8)
              size_map = 8
              flash = 8192
              flashimageoptions += --flash_size 8MB
              ifeq ($(app), 2)
                addr_upgrade = 0x101000
              endif
            else
              ifeq ($(SPI_SIZE_MAP), 9)
                size_map = 9
                flash = 16384
                flashimageoptions += --flash_size 16MB
                ifeq ($(app), 2)
                  addr_upgrade = 0x101000
                endif
              else
                size_map = 0
                flash = 512
                flashimageoptions += --flash_size 512KB
                ifeq ($(app), 2)
                  addr_upgrade = 0x41000
                endif
              endif
            endif
          endif
        endif
      endif
    endif
  endif
endif

# --------------------------------------------------------------------------
# compiler flags using during compilation of source files

CFLAGS   += \
            -std=gnu99\
            -Werror \
            -Wpointer-arith \
            -Wundef \
            -Wl,-EL \
            -fno-inline-functions \
            -nostdlib \
            -mlongcalls \
            -mtext-section-literals \
            -ffunction-sections \
            -fdata-sections \
            -fno-builtin-printf \
            -fno-jump-tables \
            -Wno-address \
            -Wno-error=unused-function \
            -Wno-error=unused-but-set-variable \
            -Wno-error=unused-variable \
            -Wno-error=deprecated-declarations \
            -Wextra \
            -Wno-unused-parameter \
            -Wno-sign-compare \
            -Wno-missing-field-initializers \
            -D__ets__ \
            -DICACHE_FLASH

#            -mno-serialize-volatile \

CXXFLAGS += \
            -g \
            -Wpointer-arith \
            -Wundef \
            -Werror \
            -Wl,-EL \
            -fno-inline-functions \
            -nostdlib \
            -mlongcalls \
            -mtext-section-literals \
            -mno-serialize-volatile \
            -D__ets__ \
            -fno-rtti \
            -fno-exceptions \
            -DICACHE_FLASH

ifeq ("$(DEBUG_USE_GDB)", "yes")
   CFLAGS   += \
               -Og \
               -ggdb \
               -DDEBUG_USE_GDB

   CXXFLAGS += \
               -Og \
               -ggdb \
               -DDEBUG_USE_GDB
else
   CFLAGS   += \
               -Os

   CXXFLAGS += \
               -Os
endif

# linker flags used to generate the main object file
LDFLAGS += \
        -nostdlib \
        -Wl,--no-check-sections \
        -u call_user_start \
        -Wl,-static

LDFLAGS += \
        -Wl,--print-map \
        -Wl,-Map=$(TARGET_MAP)

# linker script used for the linker step
LD_SCRIPT = eagle.app.v6.ld

# --------------------------------------------------------------------------

ifneq ($(boot), none)
  ifneq ($(app),0)
    ifneq ($(findstring $(size_map),  6  8  9),)
     LD_SCRIPT = eagle.app.v6.$(boot).2048.ld
    else
      ifeq ($(size_map), 5)
       LD_SCRIPT = eagle.app.v6.$(boot).2048.ld
      else
        ifeq ($(size_map), 4)
         LD_SCRIPT = eagle.app.v6.$(boot).1024.app$(app).ld
        else
          ifeq ($(size_map), 3)
           LD_SCRIPT = eagle.app.v6.$(boot).1024.app$(app).ld
          else
            ifeq ($(size_map), 2)
             LD_SCRIPT = eagle.app.v6.$(boot).1024.app$(app).ld
            else
              ifeq ($(size_map), 0)
               LD_SCRIPT = eagle.app.v6.$(boot).512.app$(app).ld
              endif
            endif
          endif
        endif
      endif
    endif
    BIN_NAME = user$(app).$(flash).$(boot).$(size_map)
    CFLAGS += -DAT_UPGRADE_SUPPORT
  endif
else
    app = 0
endif

# --------------------------------------------------------------------------
# various paths from the SDK used in this project

SDK_LIBDIR      += lib/
SDK_LDDIR       += ld/
SDK_INCDIR      += include include/json

# --------------------------------------------------------------------------
# select which tools to use as compiler, librarian and linker

CC              := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
CXX             := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-g++
AR              := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD              := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCOPY         := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDUMP         := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump
SIZE            := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-size
NM              := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-nm

# --------------------------------------------------------------------------
# no user configurable options below here

SRC_DIR         := $(addprefix $(SOURCE_DIR_BASE),$(MODULES))
BUILD_DIR       := $(addprefix $(BUILD_DIR_BASE),$(MODULES))

MODULE_INCDIR   := $(addsuffix /include,$(SRC_DIR))
MODULE_INCDIR   := $(addprefix -I,$(MODULE_INCDIR))

INCDIR          := $(addprefix -I,$(INCDIR))
EXTRA_INCDIR    := $(addprefix -I,$(EXTRA_INCDIR))

SDK_INCDIR      := $(addprefix -I$(SDK_BASE),$(SDK_INCDIR))
SDK_LIBDIR      := $(addprefix $(SDK_BASE),$(SDK_LIBDIR))

C_SRC           := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
CPP_SRC         := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
ASM_SRC         := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.S))
SRC             := $(C_SRC) $(CPP_SRC) $(ASM_SRC)
C_OBJ           := $(patsubst %.c,%.o,$(C_SRC))
CXX_OBJ         := $(patsubst %.cpp,%.o,$(CPP_SRC))
ASM_OBJ         := $(patsubst %.S,%.o,$(ASM_SRC))
OBJ             := $(patsubst $(SOURCE_DIR_BASE)%.o,$(BUILD_DIR_BASE)%.o,$(C_OBJ) $(CPP_OBJ) $(ASM_OBJ))

LIBS            := $(addprefix -l,$(LIBS))
APP_AR          := $(addprefix $(BUILD_DIR_BASE),$(TARGET)_app.a)
TARGET_OUT      := $(addprefix $(BUILD_DIR_BASE),$(TARGET).out)
TARGET_MAP      := $(addprefix $(BUILD_DIR_BASE),$(TARGET).map)

_LD_SCRIPT      := $(addprefix -T$(SDK_BASE)$(SDK_LDDIR),$(LD_SCRIPT))

ifneq ("$(EXTRA_LD_SCRIPTS)","")
   _EXTRA_LD_SCRIPTS := $(addprefix -T,$(EXTRA_LD_SCRIPTS))
endif

# --------------------------------------------------------------------------
# we create two different files for uploading into the flash
# these are the names and options to generate them

FW_FILE_1_ADDR = 0x00000
FW_FILE_2_ADDR = 0x10000

FW_FILE_1       := $(addprefix $(FW_DIR_BASE),$(TARGET)_$(FW_FILE_1_ADDR).bin)
FW_FILE_2       := $(addprefix $(FW_DIR_BASE),$(TARGET)_$(FW_FILE_2_ADDR).bin)

# --------------------------------------------------------------------------
# V=1 print the command with not completly resolved parameter
# V=2 print the command with resolved parameter

VERBOSE ?=
V ?= $(VERBOSE)
ifeq ("$(V)","1")
   Q :=
   vecho := @true
   cecho := @true
else
  ifeq ("$(V)","2")
      Q := @
      vecho := @true
      cecho := @echo
  else
      Q := @
      vecho := @echo
      cecho := @true
   endif
endif

vpath %.c $(SRC_DIR)
vpath %.cpp $(SRC_DIR)
vpath %.S $(SRC_DIR)

# --------------------------------------------------------------------------
#  The Rules
# --------------------------------------------------------------------------

define compile-objects
#$1/%.o: %.c
$(BUILD_DIR_BASE)$1/%.o: $$(SOURCE_DIR_BASE)$1/%.c
	$(vecho) "CC $$<"
	$(cecho) "$(CC) -MMD -MP $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $$(CFLAGS) -c $$< -o $$@"
	$(Q)      $(CC) -MMD -MP $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $$(CFLAGS) -c $$< -o $$@
$(BUILD_DIR_BASE)$1/%.o: $$(SOURCE_DIR_BASE)$1/%.cpp
	$(vecho) "C+ $$<"
	$(cecho) "$(CXX) -MMD -MP $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CXXFLAGS)  -c $$< -o $$@"
	$(Q)      $(CXX) -MMD -MP $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CXXFLAGS)  -c $$< -o $$@
$(BUILD_DIR_BASE)$1/%.o: $$(SOURCE_DIR_BASE)$1/%.S
	$(vecho) "ASM $$<"
	$(cecho) "$(CC) -D__ASSEMBLER__ -MMD -MP $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $$(CFLAGS) -c $$< -o $$@"
	$(Q)      $(CC) -D__ASSEMBLER__ -MMD -MP $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $$(CFLAGS) -c $$< -o $$@
endef

# --------------------------------------------------------------------------
#  The Build Targets
# --------------------------------------------------------------------------

.PHONY:  $(PHONIES) all checkdirs flash clean

all: checkdirs $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)

# Include all dependency files already generated
DEPS := $(OBJ:.o=.d)
-include $(DEPS)

$(FW_DIR_BASE)%.bin: $(TARGET_OUT) | $(FW_DIR_BASE)
	$(vecho) "FW $(FW_DIR_BASE)"
	$(Q) $(ESPTOOL) --chip esp8266 elf2image -o $(FW_DIR_BASE)$(TARGET)_ $(TARGET_OUT)
	$(vecho) "Program size $$(stat -c '%s' $(FW_DIR_BASE)$(TARGET)_0x10000.bin) allowed $$(( $(MAX_PROG_SIZE) )) "
	$(Q) if [ $$(stat -c '%s' $(FW_DIR_BASE)$(TARGET)_0x10000.bin) -gt $$(( $(MAX_PROG_SIZE) )) ]; then echo "program binary too big!"; false; fi

ifeq ($(app), 0)
	$(vecho) "No boot needed."
else
    ifneq ($(boot), new)
	$(vecho) "Support boot_v1.2 and +"
    else
        ifneq ($(findstring $(size_map),  6  8  9),)
		$(vecho) "Support boot_v1.7 and +"
        else
            ifeq ($(size_map), 5)
		$(vecho) "Support boot_v1.6 and +"
            else
		$(vecho) "Support boot_v1.2 and +"
            endif
        endif
    endif
endif
	$(vecho) "Done"

$(TARGET_OUT): checkdirs $(USER_LIBS) $(APP_AR) $(EXTRA_LD_SCRIPTS) $(COMPONENT_LINKER_DEPS)
	$(vecho) "Linking ..."
#	$(vecho) "LD $@"
	$(Q) $(LD) $(LDFLAGS) $(EXTRA_LDFLAGS) -L$(SDK_LIBDIR) $(_LD_SCRIPT) $(_EXTRA_LD_SCRIPTS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@

#	$(Q) $(OBJDUMP) -h $@
	$(Q) $(SIZE)  $@
	$(Q) $(NM) -g $@ > _$(TARGET).sym;sort _$(TARGET).sym > $(FW_DIR_BASE)$(TARGET).sym;rm _$(TARGET).sym
	$(vecho) .

$(APP_AR): $(OBJ)
#	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FW_DIR_BASE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FW_DIR_BASE):
	$(Q) mkdir -p $@
	$(Q) mkdir -p $@/upgrade

# ===============================================================
# From http://bbs.espressif.com/viewtopic.php?f=10&t=305
# master-device-key.bin is only need if using espressive services
# master_device_key.bin 0x3e000 is not used , write blank
# See 2A-ESP8266__IOT_SDK_User_Manual__EN_v1.1.0.pdf
# http://bbs.espressif.com/download/file.php?id=532
#
# System parameter area is the last 16KB of flash
# 512KB  flash - system parameter area starts from 0x7C000
#        download blank.bin to 0x7E000 as initialization.
# 1024KB flash - system parameter area starts from 0xFC000
#        download blank.bin to 0xFE000 as initialization.
# 2048KB flash - system parameter area starts from 0x1FC000
#        download blank.bin to 0x1FE000 as initialization.
# 4096KB flash - system parameter area starts from 0x3FC000
#        download blank.bin to 0x3FE000 as initialization.
# ===============================================================

# FLASH SIZE
flashinit:
	$(vecho) "Flash init data default and blank data to $(INITDATAPOS) (common_nonos)."
	$(vecho) "$(ESPTOOL) $(ESPTOOL_OPTS) write_flash $(flashimageoptions) $(INITDATAPOS) $(SDK_BASE)bin/esp_init_data_default_v08.bin"
	$(ESPTOOL) $(ESPTOOL_OPTS) write_flash $(flashimageoptions) $(INITDATAPOS) $(SDK_BASE)bin/esp_init_data_default_v08.bin

rebuild: clean all

fp:
	@echo "TARGET_DEVICE   $(TARGET_DEVICE)"
	@echo "SPI_FLASH_SIZE  $(SPI_FLASH_SIZE)"
	@echo "SPI_SIZE_MAP    $(SPI_SIZE_MAP)"
	@echo "ESP_SPI_FLASH_SIZE_K $(ESP_SPI_FLASH_SIZE_K)"
	@echo "BLANKPOS        $(BLANKPOS)"
	@echo "INITDATAPOS     $(INITDATAPOS)"
	@echo "MAX_PROG_SIZE   $(MAX_PROG_SIZE)"
	@echo "ESPFS_POS       $(ESPFS_POS)"
	@echo "ESPFS_SIZE      $(ESPFS_SIZE)"

flags:
	@echo "CFLAGS          $(CFLAGS)"

size:
	$(Q) $(SIZE)  $(TARGET_OUT)

objdump:
	$(Q) $(OBJDUMP) -h $(TARGET_OUT)

check:
# ifeq ($(app), 0)
	@echo "Program size $$(stat -c '%s' $(FW_DIR_BASE)$(TARGET)_0x10000.bin) allowed $$(( $(MAX_PROG_SIZE) )) "
	$(Q) if [ $$(stat -c '%s' $(FW_DIR_BASE)$(TARGET)_0x10000.bin) -gt $$(( $(MAX_PROG_SIZE) )) ]; then echo "program binary too big!"; false; fi

	@echo "EPSFS size $$(stat -c '%s' $(BUILD_DIR_BASE)libesphttpd/webpages.espfs) allowed $$(( $(ESPFS_SIZE) )) "
	$(Q) if [ $$(stat -c '%s' $(BUILD_DIR_BASE)libesphttpd/webpages.espfs) -gt $$(( $(ESPFS_SIZE) )) ]; then echo "webpages.espfs too big!"; false; fi
# else
#   ifeq ($(boot), none)
#   else
#   endif
# endif

showdirs:
	@echo "-----------------------------------------"
	@echo "    TARGET:         "$(TARGET)
	@echo "    TARGET_DEVICE:  "$(TARGET_DEVICE)
	@echo "    TARGET_OUT:     "$(TARGET_OUT)
	@echo "    USER_LIBS:      "$(USER_LIBS)
	@echo "    BUILD_DIR_BASE: "$(BUILD_DIR_BASE)
	@echo "    BUILD_DIR:      "$(BUILD_DIR)
	@echo "    FW_DIR_BASE:    "$(FW_DIR_BASE)
	@echo "    SRC_DIR:        "$(SRC_DIR)
	@echo "    C_SRC:          "$(C_SRC)
	@echo "    CPP_SRC:        "$(CPP_SRC)
	@echo "    ASM_SRC:        "$(ASM_SRC)
	@echo "    SRC:            "$(SRC)
	@echo "    APP_AR:         "$(APP_AR)
	@echo "-----------------------------------------"
	@echo "    INCDIR:         "$(INCDIR)
	@echo "    EXTRA_INCDIR:   "$(EXTRA_INCDIR)
	@echo "    MODULE_INCDIR:  "$(MODULE_INCDIR)
	@echo "-----------------------------------------"
	@echo "    C_OBJ:          "$(C_OBJ)
	@echo "    CPP_OBJ:        "$(CPP_OBJ)
	@echo "    ASM_OBJ:        "$(ASM_OBJ)
	@echo "    OBJ:            "$(OBJ)
	@echo "    DEPS:           "$(DEPS)
	@echo "-----------------------------------------"

# --------------------------------------------------------------------------
#  The Build the Modules
# --------------------------------------------------------------------------

$(foreach bdir,$(MODULES),$(eval $(call compile-objects,$(bdir))))

# --------------------------------------------------------------------------
#
# --------------------------------------------------------------------------
