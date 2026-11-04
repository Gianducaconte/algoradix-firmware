# Algoradix firmware — bare-metal STM32F427, GNU make
PROJECT  := algoradix
TRGT     := arm-none-eabi-
CC       := $(TRGT)gcc
OBJCOPY  := $(TRGT)objcopy
OBJDUMP  := $(TRGT)objdump
SIZE     := $(TRGT)size

# Stage selector: 5 = the patch host (default: engines are .APX files on the card, see patchsdk/), 3 = the previous synth with all
# engines built in (release 09, kept as a fallback), 0 heartbeat, 1 panel test, 2 audio test, 4 SD test
STAGE    ?= 5
BUILD    := build/stage$(STAGE)
LDSCRIPT := $(if $(filter 5,$(STAGE)),STM32F427xG_host.ld,STM32F427xG.ld)

SRC_C    := $(wildcard src/*.c) $(wildcard hal/*.c) $(wildcard panel/*.c) \
            $(wildcard dsp/*.c) $(wildcard patches/*.c) $(wildcard ui/*.c) $(wildcard app/*.c) \
            $(wildcard fatfs/*.c) $(wildcard sampler/*.c) \
            tinyusb/src/tusb.c tinyusb/src/common/tusb_fifo.c $(wildcard tinyusb/src/host/*.c) \
            tinyusb/src/device/usbd.c tinyusb/src/class/msc/msc_device.c tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c \
            patchsdk/apx_inst.c \
            tinyusb/src/class/midi/midi_host.c tinyusb/src/portable/synopsys/dwc2/hcd_dwc2.c tinyusb/src/portable/synopsys/dwc2/dwc2_common.c
INCS     := -I. -Iinclude -Icmsis -Itinyusb -Itinyusb/src -Ipatchsdk -Ifatfs -Isampler -Iboard -Isrc -Ihal -Ipanel -Idsp -Ipatches -Iui -Iapp

MCUFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS     := -DSTM32F427xx -DSTAGE=$(STAGE)
OPT      ?= -O2
CFLAGS   := $(MCUFLAGS) $(DEFS) $(INCS) $(OPT) -g3 -std=c11 \
            -ffreestanding -fno-common -ffunction-sections -fdata-sections \
            -fno-strict-aliasing -fsingle-precision-constant \
            -Wall -Wextra -Wshadow -Wundef -Wdouble-promotion -Wno-unused-parameter \
            -MMD -MP
LDFLAGS  := $(MCUFLAGS) -nostartfiles -nostdlib -static \
            -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(PROJECT).map -Wl,--print-memory-usage \
            -T $(LDSCRIPT)
LIBS     := -lgcc

OBJS     := $(patsubst %.c,$(BUILD)/obj/%.o,$(SRC_C))
DEPS     := $(OBJS:.o=.d)

ELF := $(BUILD)/$(PROJECT).elf
BIN := $(BUILD)/$(PROJECT).bin
HEX := $(BUILD)/$(PROJECT).hex
LST := $(BUILD)/$(PROJECT).lst

.PHONY: all clean size flash dfu-list backup

all: $(BIN) $(HEX) $(LST) size

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(LST): $(ELF)
	$(OBJDUMP) -d -S $< > $@

size: $(ELF)
	$(SIZE) $<

clean:
	rm -rf $(BUILD)

# ---- DFU (STM32 ROM bootloader, hold S1 while plugging the mini-USB) ----
DFU_ID := 0483:df11

dfu-list:
	dfu-util -l

# Read the whole 1 MB application flash before the first ever flash.
backup:
	@mkdir -p ../backup
	dfu-util -d $(DFU_ID) -a 0 -s 0x08000000:1048576 -U ../backup/flash-$(shell date +%Y%m%d-%H%M%S).bin

flash: $(BIN)
	dfu-util -d $(DFU_ID) -a 0 -s 0x08000000:leave -D $(BIN)

-include $(DEPS)
