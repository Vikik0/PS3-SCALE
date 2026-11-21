#!/usr/bin/env python3
"""Adapt the PS3 PCSX-ReARMed core for RetroArch's statically linked PSL1GHT build."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_psxonps3_retroarch.py <pcsx-rearmed-source>")

root = Path(sys.argv[1])

# This package loads BIN/CUE content through the libretro file interface.
# It does not use the physical PS3 BD/DVD drive or the rcdrom_* front-end API.
makefile = root / "Makefile.libretro"
text = makefile.read_text()
old_async = "\tUSE_ASYNC_CDROM = 1\n  libpcsxcore/cdrom-async.o frontend/libretro.o: CFLAGS += -DHAVE_CDROM\n"
new_async = "\tUSE_ASYNC_CDROM = 0\n"
if old_async not in text:
    raise SystemExit("Expected PSL1GHT physical-CD-ROM block was not found")
makefile.write_text(text.replace(old_async, new_async, 1))

# RetroArch already links libretro-common/rthreads/rthreads.c into griffin.o.
# The PS3-specific PCSX thread shim used to define the same sthread/slock/scond
# symbols a second time. Keep PCSX's thread-type entry point but delegate to
# RetroArch's rthreads implementation, using the former 256 KiB PS3 stack.
threads = root / "frontend/pcsxr-threads.c"
text = threads.read_text()
start = text.find("#if defined(__lv2ppu__)")
end_marker = "#elif !defined(USE_C11_THREADS)"
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("Could not locate the PS3 pcsxr-threads implementation")
replacement = """#if defined(__lv2ppu__)

/* Reuse RetroArch's rthreads symbols to avoid duplicate sthread/slock/scond
 * definitions when the core is linked into the RetroArch PS3 executable. */
extern sthread_t *sthread_create_with_stack_size(
      void (*thread_func)(void *), void *userdata, size_t stack_size);

sthread_t *pcsxr_sthread_create(void (*thread_func)(void *),
      enum pcsxr_thread_type type)
{
   (void)type;
   return sthread_create_with_stack_size(thread_func, NULL, 256 * 1024);
}

#define CORE_COUNT() 2
#define pcsxr_sthread_lib_init()

#elif !defined(USE_C11_THREADS)"""
text = text[:start] + replacement + text[end + len(end_marker):]
threads.write_text(text)

print("Applied RetroArch PSL1GHT integration fixes: file-based CD-ROM and shared rthreads.")
