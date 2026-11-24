# MANUAL, READ-ONLY dump. Run only while Ghosts Campaign is active.
# Campaign and Multiplayer share BLUS31270, so do not auto-run this script
# from wm_ingame. The operator must select the correct game mode first.
# Run from webMAN: http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_campaign_tu116.bat
# Requires webMAN MOD and PS3MAPI enabled.
#
# PS3MAPI's omitted proc= uses the current game PID while in-game.
# dump= is the number of bytes to dump; webMAN writes 64 KiB-padded chunks.

logfile /dev_hdd0/GHOSTS_TU116_CAMPAIGN_DUMP.log

if titleid BLUS31270
  if not exist /dev_hdd0/GHOSTS_TU116_CAMPAIGN_DUMP_DONE
    log Starting Campaign-only read-only memory dump
    wait 3
    del /dev_hdd0/mem_dump.bin
    del /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG0.bin
    del /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG1.bin

    # Campaign default.self PT_LOAD 0: vaddr 0x10000, p_filesz 0x7C2428.
    /getmem.ps3mapi?addr=10000&len=20&dump=7c2428
    abort if not exist /dev_hdd0/mem_dump.bin
    ren /dev_hdd0/mem_dump.bin=/dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG0.bin
    abort if not exist /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG0.bin
    log Campaign segment 0 saved and verified present

    # Campaign default.self PT_LOAD 1: vaddr 0x7E0000, p_filesz 0x1D1E0.
    /getmem.ps3mapi?addr=7e0000&len=20&dump=1d1e0
    abort if not exist /dev_hdd0/mem_dump.bin
    ren /dev_hdd0/mem_dump.bin=/dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG1.bin
    abort if not exist /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG1.bin
    log Campaign segment 1 saved and verified present

    logfile /dev_hdd0/GHOSTS_TU116_CAMPAIGN_DUMP_DONE
    log complete
    popup Ghosts Campaign TU116 dump complete
  else
    log Campaign dump already completed; skipping
  end if
else
  log Wrong title ID; no memory was read
end if
