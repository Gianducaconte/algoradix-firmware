# Builds Algoradix patches (.APX) for the SD card. Same compiler and flags as the firmware; every patch
# is linked on its own at the fixed load address (apx.ld) and finished by tools/apx_pack.py.
#   make            the three demo patches -> build/card/ALGORADIX/PATCHES/*.APX (+ demo sample folders)
#   make install CARD=/Volumes/NAME     copy build/card/ALGORADIX onto the card
TRGT    := arm-none-eabi-
CC      := $(TRGT)gcc
OBJCOPY := $(TRGT)objcopy
FW      := ..
BUILD   := build
CARDDIR := $(BUILD)/card/ALGORADIX
MCU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
# whole-program optimisation: the per-sample chain (oscillators, envelopes, filter) spans several files and
# must be inlined into the voice loop. Measured on the board: 12 % DSP per voice at -O2 without LTO.
OPT     := -O3 -flto -ffast-math
CFLAGS  := $(MCU) -DAPX_PATCH -I. -I$(FW)/include -I$(FW)/dsp -I$(FW)/patches $(OPT) -std=c11 -ffreestanding -fno-common \
           -ffunction-sections -fdata-sections -fno-strict-aliasing -fsingle-precision-constant \
           -Wall -Wextra -Wshadow -Wundef -Wdouble-promotion -Wno-unused-parameter -MMD -MP
LDFLAGS := $(MCU) $(OPT) -nostartfiles -nostdlib -static -Wl,--gc-sections -T apx.ld
LIB_SRC := $(wildcard $(FW)/dsp/*.c) $(FW)/patches/pool.c $(FW)/src/libc_min.c apx_inst.c

PATCHES      := 01BASS 02LEAD 03WAVSYN
SRC_01BASS   := demo/bass.c
SRC_02LEAD   := demo/lead.c
SRC_03WAVSYN := demo/wavsynth.c
# your own patches: add  PATCHES += NAME  and  SRC_NAME := path/to/file.c  (see PATCH-GUIDE.md)

ifeq ($(READOUT),1)
CFLAGS += -DAPX_LOAD_READOUT
endif

# banks: BANK_<FILE> := <folder> puts that patch into /ALGORADIX/PATCHES/<folder>/ (one bank per folder, shown without its
# leading number: "3-Pad" = Pad). No BANK_ = the folder itself = the bank "Core".
apxpath = $(CARDDIR)/PATCHES/$(if $(BANK_$(1)),$(BANK_$(1))/)$(1).APX
obj = $(patsubst %.c,$(BUILD)/obj/%.o,$(subst ../,up/,$(1)))
LIB_OBJ := $(call obj,$(LIB_SRC))
APX     := $(foreach p,$(PATCHES),$(call apxpath,$(p)))

# templates: compile-only proof for the examples in PATCH-GUIDE.md (never installed)
TEMPLATES      := TPLEFFECT TPLINSTR
SRC_TPLEFFECT  := templates/effect_template.c
SRC_TPLINSTR   := templates/instrument_template.c

.PHONY: all clean install demo-wavs templates
all: $(APX) demo-wavs

$(BUILD)/obj/up/%.o: ../%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(if $(findstring libc_min,$<),-fno-lto -O2) -c $< -o $@
$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

define PATCH_RULE
$(BUILD)/$(1).elf: $$(call obj,$$(SRC_$(1))) $(LIB_OBJ) apx.ld
	@$(CC) $(LDFLAGS) $$(call obj,$$(SRC_$(1))) $(LIB_OBJ) -lgcc -Wl,-Map=$(BUILD)/$(1).map -o $$@
$(call apxpath,$(1)): $(BUILD)/$(1).elf
	@mkdir -p $$(dir $$@)
	@$(OBJCOPY) -O binary -j .image $$< $(BUILD)/$(1).bin
	@python3 ../../tools/apx_pack.py $(BUILD)/$(1).bin $$@
endef
$(foreach p,$(PATCHES) $(TEMPLATES),$(eval $(call PATCH_RULE,$(p))))
templates: $(foreach p,$(TEMPLATES),$(CARDDIR)/PATCHES/$(p).APX)
	@rm -f $(foreach p,$(TEMPLATES),$(CARDDIR)/PATCHES/$(p).APX)

demo-wavs:
	@python3 ../../tools/make_demo_wavs.py $(CARDDIR)/SAMPSY

install: all
	@test -d "$(CARD)" || (echo "usage: make install CARD=/Volumes/<card>"; exit 1)
	@for p in $(foreach p,$(PATCHES),$(if $(BANK_$(p)),$(p))); do rm -f "$(CARD)/ALGORADIX/PATCHES/$$p.APX"; done
	mkdir -p "$(CARD)/ALGORADIX" && cp -R $(CARDDIR)/ "$(CARD)/ALGORADIX/"
	@echo "copied. EJECT THE CARD ON THE MAC BEFORE LEAVING DISK MODE."
clean:
	rm -rf $(BUILD)
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
