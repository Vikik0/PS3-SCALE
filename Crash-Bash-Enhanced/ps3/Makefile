SHELL := /bin/bash

PS3DEV ?=
PS3DK ?= $(PS3DEV)/ps3dk
BUILD := build
PPU_CC := $(PS3DEV)/ppu/bin/powerpc64-ps3-elf-gcc
PRX_GEN := $(PS3DEV)/bin/prx-gen
FSELF := $(PS3DEV)/bin/fself
CRT := $(PS3DK)/ppu/lib/lv2-prx-crt.o
CRT_I := $(PS3DK)/ppu/lib/lv2-crti.o
CRT_INIT := $(PS3DK)/ppu/lib/lv2-prx-crt1.o
PRX_LD_SOURCE := $(PS3DK)/ppu/lib/lv2-prx.ld
PRX_LD := $(BUILD)/lv2-prx.ld
PRX_SPECS := $(PS3DK)/ppu/lib/lv2-prx.specs

CFLAGS := -O2 -Wall -Wextra -Werror -std=gnu11 -mcpu=cell -mhard-float \
          -ffunction-sections -fno-stack-protector -fno-builtin \
          -D_POSIX_C_SOURCE=200112L -DBASH_VERSION='"1.4.0"' -I$(CURDIR)/compat -I$(PS3DK)/ppu/include
NET_LIBS := -L$(PS3DK)/ppu/lib -Wl,--start-group -lio_stub -lnet_stub \
            -lnetctl_stub -llv2_stub -lsysmodule_stub -lrt -lfs_stub -lc_stub -lc -lsysbase \
            -Wl,--end-group -lgcc

.PHONY: all clean check

all: $(BUILD)/crash_bash_input.fake.sprx

$(BUILD):
	mkdir -p $@

# The PPC64 TOC anchor is .got + 0x8000. Extend the loadable segment through
# the full TOC-relative data window so prx-gen can resolve BSS relocations.
$(PRX_LD): $(PRX_LD_SOURCE) | $(BUILD)
	python3 -c 'from pathlib import Path; import sys; src=Path(sys.argv[1]); text=src.read_text(); needle=chr(9)+"_end = .;"; assert text.count(needle) == 1; pad=chr(9)+".toc_anchor_pad MAX(., .TOC.) : { . += 0x10000; } : hdr_load"; Path(sys.argv[2]).write_text(text.replace(needle, pad+chr(10)+needle, 1))' $(PRX_LD_SOURCE) $@

$(BUILD)/module.o: module.c input_hook.h net_adapter.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/input_hook.o: input_hook.c input_hook.h net_adapter.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/net_adapter.o: net_adapter.c net_adapter.h input_hook.h relay_client.h compat/SDL3/SDL.h compat/ifaddrs.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -Wno-error -I../upstream/crash_bash_decomp/port/platform -c $< -o $@

$(BUILD)/netdb_compat.o: netdb_compat.c compat/netdb.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/thread_compat.o: thread_compat.c | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ws_client.o: ws_client.c ws_client.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/relay_client.o: relay_client.c relay_client.h ws_client.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crash_bash_input.elf: $(BUILD)/module.o $(BUILD)/input_hook.o $(BUILD)/net_adapter.o $(BUILD)/netdb_compat.o $(BUILD)/thread_compat.o $(BUILD)/ws_client.o $(BUILD)/relay_client.o $(CRT_I) $(CRT) $(CRT_INIT) $(PRX_LD) $(PRX_SPECS)
	$(PPU_CC) -nostdlib -nostartfiles -specs=$(PRX_SPECS) \
		-Wl,-q -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-T,$(PRX_LD) \
		$(BUILD)/module.o $(BUILD)/input_hook.o $(BUILD)/net_adapter.o $(BUILD)/netdb_compat.o $(BUILD)/thread_compat.o $(BUILD)/ws_client.o $(BUILD)/relay_client.o $(CRT_I) $(CRT) $(CRT_INIT) $(NET_LIBS) -o $@

$(BUILD)/crash_bash_input.prx: $(BUILD)/crash_bash_input.elf
	$(PRX_GEN) build $< -o $@ --name crash_bash_input --version 0.2 || { \
		echo "PRX relocation diagnostics:"; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -lW $<; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -SW $<; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -rW $< | grep -B3 -A3 "0000000000003212" || true; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-nm -n $< | grep -B4 -A4 -i "195ce8" || true; \
		exit 1; \
	}
	$(PRX_GEN) check $@

$(BUILD)/crash_bash_input.fake.sprx: $(BUILD)/crash_bash_input.prx
	$(FSELF) $< $@

check: $(BUILD)/crash_bash_input.prx
	$(PRX_GEN) check $<

clean:
	rm -rf $(BUILD)
