# MANUAL, READ-ONLY dump. Run only while Ghosts Multiplayer is active.
# Campaign and Multiplayer share BLUS31270, so do not auto-run this script
# from wm_ingame. The operator must select the correct game mode first.
# Run from webMAN: http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_multiplayer_tu116.bat
# Requires webMAN MOD and PS3MAPI enabled.
#
# PS3MAPI's omitted proc= uses the current game PID while in-game.
# dump= is the number of bytes to dump; webMAN writes 64 KiB-padded chunks.

logfile /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_DUMP.log

if titleid BLUS31270
  if not exist /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_DUMP_DONE
    log Starting Multiplayer-only read-only memory dump
    wait 3
    del /dev_hdd0/mem_dump.bin
    del /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_SEG0.bin
    del /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_SEG1.bin

    # Multiplayer default_mp.self PT_LOAD 0: vaddr 0x10000, p_filesz 0xA48C28.
    /getmem.ps3mapi?addr=10000&len=20&dump=a48c28
    abort if not exist /dev_hdd0/mem_dump.bin
    ren /dev_hdd0/mem_dump.bin=/dev_hdd0/GHOSTS_TU116_MULTIPLAYER_SEG0.bin
    log Multiplayer segment 0 captured

    # Multiplayer default_mp.self PT_LOAD 1: vaddr 0xA60000, p_filesz 0xE39D0.
    /getmem.ps3mapi?addr=a60000&len=20&dump=e39d0
    abort if not exist /dev_hdd0/mem_dump.bin
    ren /dev_hdd0/mem_dump.bin=/dev_hdd0/GHOSTS_TU116_MULTIPLAYER_SEG1.bin
    log Multiplayer segment 1 captured

    logfile /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_DUMP_DONE
    log complete
    popup Ghosts Multiplayer TU116 dump complete
  else
    log Multiplayer dump already completed; skipping
  end if
else
  log Wrong title ID; no memory was read
end if
