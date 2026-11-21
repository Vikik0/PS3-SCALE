SHELL := /bin/bash

PS3DEV ?=
PS3DK ?= $(PS3DEV)/ps3dk
BUILD := build
MBEDTLS_BUILD := $(BUILD)/mbedtls
MBEDTLS_A := $(MBEDTLS_BUILD)/libcb_mbedtls.a
MBEDTLS_INC := $(MBEDTLS_BUILD)/include
LOADER_BUILD := $(BUILD)/loader
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
          -D_POSIX_C_SOURCE=200112L -DBASH_VERSION='"1.4.0"' -DMBEDTLS_CONFIG_FILE='"cb_mbedtls_config.h"' -I$(CURDIR) -I$(CURDIR)/compat -I$(PS3DK)/ppu/include -I$(MBEDTLS_INC)
NET_LIBS := -L$(PS3DK)/ppu/lib -Wl,--start-group -lio_stub -lnet_stub \
            -lnetctl_stub -llv2_stub -lsysmodule_stub -lsysutil_stub -lrt -lfs_stub -lc_stub -lc -lsysbase \
            -Wl,--end-group -lgcc

.PHONY: all clean check

all: $(BUILD)/crash_bash_input.fake.sprx $(LOADER_BUILD)/crash_bash_loader.fake.sprx

$(BUILD):
	mkdir -p $@

# The PPC64 TOC anchor is .got + 0x8000. Extend the loadable segment through
# the full TOC-relative data window so prx-gen can resolve BSS relocations.
$(PRX_LD): $(PRX_LD_SOURCE) | $(BUILD)
	python3 -c 'from pathlib import Path; import sys; src=Path(sys.argv[1]); text=src.read_text(); needle=chr(9)+"_end = .;"; assert text.count(needle) == 1; pad=chr(9)+".toc_anchor_pad MAX(., .TOC.) : { . += 0x10000; } : hdr_load"; Path(sys.argv[2]).write_text(text.replace(needle, pad+chr(10)+needle, 1))' $(PRX_LD_SOURCE) $@

$(BUILD)/module.o: module.c input_hook.h net_adapter.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/input_hook.o: input_hook.c input_hook.h net_adapter.h relay_ui.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/net_adapter.o: net_adapter.c net_adapter.h input_hook.h relay_client.h compat/SDL3/SDL.h compat/ifaddrs.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -Wno-error -I../upstream/crash_bash_decomp/port/platform -c $< -o $@

$(BUILD)/relay_ui.o: relay_ui.c relay_ui.h input_hook.h net_adapter.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/netdb_compat.o: netdb_compat.c compat/netdb.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/thread_compat.o: thread_compat.c | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/errno_compat.o: errno_compat.c | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@



$(BUILD)/ca_bundle.c: tools/embed_ca_bundle.py | $(BUILD)
	python3 tools/embed_ca_bundle.py /etc/ssl/certs/ca-certificates.crt $@

$(BUILD)/ca_bundle.o: $(BUILD)/ca_bundle.c | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/tls_client.o: tls_client.c tls_client.h cb_mbedtls_config.h $(MBEDTLS_A) | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ws_client.o: ws_client.c ws_client.h tls_client.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/relay_client.o: relay_client.c relay_client.h ws_client.h tls_client.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crash_bash_input.elf: $(BUILD)/module.o $(BUILD)/input_hook.o $(BUILD)/relay_ui.o $(BUILD)/net_adapter.o $(BUILD)/netdb_compat.o $(BUILD)/thread_compat.o $(BUILD)/errno_compat.o $(BUILD)/ws_client.o $(BUILD)/relay_client.o $(BUILD)/tls_client.o $(BUILD)/ca_bundle.o $(MBEDTLS_A) $(CRT_I) $(CRT) $(CRT_INIT) $(PRX_LD) $(PRX_SPECS)
	$(PPU_CC) -nostdlib -nostartfiles -specs=$(PRX_SPECS) \
		-Wl,-q -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-T,$(PRX_LD) \
		$(BUILD)/module.o $(BUILD)/input_hook.o $(BUILD)/relay_ui.o $(BUILD)/net_adapter.o $(BUILD)/netdb_compat.o $(BUILD)/thread_compat.o $(BUILD)/errno_compat.o $(BUILD)/ws_client.o $(BUILD)/relay_client.o $(BUILD)/tls_client.o $(BUILD)/ca_bundle.o $(MBEDTLS_A) $(CRT_I) $(CRT) $(CRT_INIT) $(NET_LIBS) -o $@

$(BUILD)/crash_bash_input.prx: $(BUILD)/crash_bash_input.elf
	$(PRX_GEN) build $< -o $@ --name crash_bash_input --version 0.2 || { \
		echo "PRX relocation diagnostics:"; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -lW $<; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -SW $<; \
		echo "TLS relocation diagnostics:"; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -rW $< | grep -E "R_PPC64_.*(TLS|TPREL|DTPREL)" || true; \
		$(PS3DEV)/ppu/bin/powerpc64-ps3-elf-readelf -sW $< | grep -E "TLS|__tls|pt_self_cookie|__getreent|__errno" || true; \
		exit 1; \
	}
	$(PRX_GEN) check $@

$(BUILD)/crash_bash_input.fake.sprx: $(BUILD)/crash_bash_input.prx
	$(FSELF) $< $@

$(LOADER_BUILD):
	mkdir -p $@

$(LOADER_BUILD)/loader.o: loader/loader.c | $(LOADER_BUILD)
	$(PPU_CC) $(CFLAGS) -I$(CURDIR) -c $< -o $@

$(LOADER_BUILD)/thread_compat.o: thread_compat.c | $(LOADER_BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(LOADER_BUILD)/crash_bash_loader.elf: $(LOADER_BUILD)/loader.o $(LOADER_BUILD)/thread_compat.o $(CRT_I) $(CRT) $(CRT_INIT) $(PRX_LD) $(PRX_SPECS)
	$(PPU_CC) -nostdlib -nostartfiles -specs=$(PRX_SPECS) \
		-Wl,-q -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-T,$(PRX_LD) \
		$(LOADER_BUILD)/loader.o $(LOADER_BUILD)/thread_compat.o $(CRT_I) $(CRT) $(CRT_INIT) $(NET_LIBS) -o $@

$(LOADER_BUILD)/crash_bash_loader.prx: $(LOADER_BUILD)/crash_bash_loader.elf
	$(PRX_GEN) build $< -o $@ --name crash_bash_loader --version 0.1
	$(PRX_GEN) check $@

$(LOADER_BUILD)/crash_bash_loader.fake.sprx: $(LOADER_BUILD)/crash_bash_loader.prx
	$(FSELF) $< $@

check: $(BUILD)/crash_bash_input.prx $(LOADER_BUILD)/crash_bash_loader.prx
	$(PRX_GEN) check $(BUILD)/crash_bash_input.prx
	$(PRX_GEN) check $(LOADER_BUILD)/crash_bash_loader.prx

clean:
	rm -rf $(BUILD)
