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
          -I$(PS3DEV)/ppu/include -I$(PS3DK)/include -I$(PS3DK_SRC)/sdk/include

.PHONY: all clean check

all: $(BUILD)/ghosts_coop.fake.sprx

$(BUILD):
	mkdir -p $@

$(BUILD)/module.o: module.c script_override.h ../local_input.h ../mission_rules.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/script_override.o: script_override.c script_override.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exports.o: exports.S | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ghc_protocol.o: ../ghc_protocol.c ../ghc_protocol.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_protocol.c -o $@

$(BUILD)/ghc_websocket.o: ../ghc_websocket.c ../ghc_websocket.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_websocket.c -o $@

$(BUILD)/mission_rules.o: ../mission_rules.c ../mission_rules.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../mission_rules.c -o $@

$(BUILD)/local_input.o: ../local_input.c ../local_input.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../local_input.c -o $@

$(BUILD)/local_player_gate.o: ../local_player_gate.c ../local_player_gate.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../local_player_gate.c -o $@

$(BUILD)/local_input_ps3.o: ../local_input_ps3.c ../local_input.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../local_input_ps3.c -o $@

$(BUILD)/ghc_stun.o: ../ghc_stun.c ../ghc_stun.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_stun.c -o $@

$(BUILD)/ghc_udp_ps3.o: ../ghc_udp_ps3.c ../ghc_udp_ps3.h ../ghc_stun.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_udp_ps3.c -o $@

$(BUILD)/ghc_game_protocol.o: ../ghc_game_protocol.c ../ghc_game_protocol.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_game_protocol.c -o $@

$(BUILD)/ghc_p2p_udp.o: ../ghc_p2p_udp.c ../ghc_p2p_udp.h ../ghc_lobby.h ../ghc_protocol.h ../ghc_udp_ps3.h ../ghc_game_protocol.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_p2p_udp.c -o $@

$(BUILD)/ghc_lobby_json.o: ../ghc_lobby_json.c ../ghc_lobby_json.h ../ghc_lobby_string.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_lobby_json.c -o $@

$(BUILD)/ghc_lobby_ps3.o: ../ghc_lobby_ps3.c ../ghc_lobby.h ../ghc_lobby_json.h ../ghc_lobby_string.h | $(BUILD)
	$(PPU_CC) $(CFLAGS) -c ../ghc_lobby_ps3.c -o $@

$(BUILD)/ghosts_coop.elf: $(BUILD)/module.o $(BUILD)/script_override.o $(BUILD)/exports.o $(BUILD)/ghc_protocol.o $(BUILD)/ghc_websocket.o $(BUILD)/mission_rules.o $(BUILD)/local_input.o $(BUILD)/local_player_gate.o $(BUILD)/local_input_ps3.o $(BUILD)/ghc_lobby_json.o $(BUILD)/ghc_lobby_ps3.o $(BUILD)/ghc_stun.o $(BUILD)/ghc_udp_ps3.o $(BUILD)/ghc_game_protocol.o $(BUILD)/ghc_p2p_udp.o $(CRT) $(PRX_LD) $(PRX_SPECS)
	$(PPU_CC) -nostdlib -nostartfiles -specs=$(PRX_SPECS) \
		-Wl,-q -Wl,--no-warn-rwx-segments -Wl,-T,$(PRX_LD) \
		$(BUILD)/module.o $(BUILD)/script_override.o $(BUILD)/exports.o $(BUILD)/ghc_protocol.o $(BUILD)/ghc_websocket.o $(BUILD)/mission_rules.o $(BUILD)/local_input.o $(BUILD)/local_player_gate.o $(BUILD)/local_input_ps3.o $(BUILD)/ghc_lobby_json.o $(BUILD)/ghc_lobby_ps3.o $(BUILD)/ghc_stun.o $(BUILD)/ghc_udp_ps3.o $(BUILD)/ghc_game_protocol.o $(BUILD)/ghc_p2p_udp.o $(CRT) \
		-L$(PS3DK)/ppu/lib -lhttp_stub -lhttp_util_stub -lssl_stub -lnet_stub -lsysmodule_stub -lsys_fs_stub -lio_stub -llv2_stub -lgcc -o $@

$(BUILD)/ghosts_coop.prx: $(BUILD)/ghosts_coop.elf
	$(PRX_GEN) build $< -o $@ --name ghosts_coop --version 0.1
	$(PRX_GEN) check $@

$(BUILD)/ghosts_coop.fake.sprx: $(BUILD)/ghosts_coop.prx
	$(FSELF) $< $@

check: $(BUILD)/ghosts_coop.prx
	$(PRX_GEN) check $<

clean:
	rm -rf $(BUILD)
