SHELL := /bin/bash

PS3DEV ?=
PS3DK ?= $(PS3DEV)/ps3dk
BUILD := build
PPU_CC := $(PS3DEV)/ppu/bin/powerpc64-ps3-elf-gcc
PRX_GEN := $(PS3DEV)/bin/prx-gen
FSELF := $(PS3DEV)/bin/fself
CRT := $(PS3DK)/ppu/lib/lv2-prx-crt.o
PRX_LD := $(PS3DK)/ppu/lib/lv2-prx.ld
PRX_SPECS := $(PS3DK)/ppu/lib/lv2-prx.specs

CFLAGS := -O2 -Wall -Wextra -Werror -std=gnu11 -mcpu=cell -mhard-float \
          -ffunction-sections -fdata-sections -fno-stack-protector -fno-builtin \
          -I$(PS3DEV)/ppu/include -I$(PS3DK)/ppu/include

.PHONY: all clean check

all: $(BUILD)/ghosts_campaign_pad_probe.fake.sprx

$(BUILD):
	mkdir -p $@

$(BUILD)/module.o: module.c | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exports.o: exports.S | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ghosts_campaign_pad_probe.elf: $(BUILD)/module.o $(BUILD)/exports.o $(CRT) $(PRX_LD) $(PRX_SPECS)
	$(PPU_CC) -nostdlib -nostartfiles -specs=$(PRX_SPECS) \
		-Wl,-q -Wl,--no-warn-rwx-segments -Wl,-T,$(PRX_LD) \
		$(BUILD)/module.o $(BUILD)/exports.o $(CRT) \
		-L$(PS3DK)/ppu/lib -L$(PS3DEV)/ppu/lib \
		-lfs_stub -lio_stub -llv2_stub -lgcc -o $@

$(BUILD)/ghosts_campaign_pad_probe.prx: $(BUILD)/ghosts_campaign_pad_probe.elf
	$(PRX_GEN) build $< -o $@ --name ghosts_campaign_pad_probe --version 0.1
	$(PRX_GEN) check $@

$(BUILD)/ghosts_campaign_pad_probe.fake.sprx: $(BUILD)/ghosts_campaign_pad_probe.prx
	$(FSELF) $< $@

check: $(BUILD)/ghosts_campaign_pad_probe.prx
	$(PRX_GEN) check $<

clean:
	rm -rf $(BUILD)
