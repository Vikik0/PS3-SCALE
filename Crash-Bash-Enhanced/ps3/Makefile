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
          -D_POSIX_C_SOURCE=200112L -I$(CURDIR)/compat -I$(PS3DK)/ppu/include
NET_LIBS := -L$(PS3DK)/ppu/lib -lio_stub -lnet_stub -lnetctl_stub \
            -llv2_stub -lsysmodule_stub -lfs_stub -lc_stub

.PHONY: all clean check

all: $(BUILD)/crash_bash_input.fake.sprx

$(BUILD):
	mkdir -p $@

$(BUILD)/module.o: module.c input_hook.h net_adapter.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/input_hook.o: input_hook.c input_hook.h net_adapter.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/net_adapter.o: net_adapter.c net_adapter.h input_hook.h compat/SDL3/SDL.h compat/ifaddrs.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -Wno-error -I../upstream/crash_bash_decomp/port/platform -c $< -o $@

$(BUILD)/crash_bash_input.elf: $(BUILD)/module.o $(BUILD)/input_hook.o $(BUILD)/net_adapter.o $(CRT) $(PRX_LD) $(PRX_SPECS)
	$(PPU_CC) -nostdlib -nostartfiles -specs=$(PRX_SPECS) \
		-Wl,-q -Wl,--no-warn-rwx-segments -Wl,-T,$(PRX_LD) \
		$(BUILD)/module.o $(BUILD)/input_hook.o $(BUILD)/net_adapter.o $(CRT) $(NET_LIBS) -o $@

$(BUILD)/crash_bash_input.prx: $(BUILD)/crash_bash_input.elf
	$(PRX_GEN) build $< -o $@ --name crash_bash_input --version 0.2
	$(PRX_GEN) check $@

$(BUILD)/crash_bash_input.fake.sprx: $(BUILD)/crash_bash_input.prx
	$(FSELF) $< $@

check: $(BUILD)/crash_bash_input.prx
	$(PRX_GEN) check $<

clean:
	rm -rf $(BUILD)
