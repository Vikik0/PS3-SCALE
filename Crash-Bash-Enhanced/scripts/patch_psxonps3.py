#!/usr/bin/env python3
"""Apply the SPU GCC vector-conversion compatibility fix to pinned PSXONPS3."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_psxonps3.py <upstream-repository>")
root = Path(sys.argv[1])
path = root / "ps3/spu/aa_kernel.h"
text = path.read_text()

if "static inline aa_vf aa_to_float" in text:
    print("SPU vector conversion patch already applied.")
    raise SystemExit(0)

def replace_once(old, new):
    global text
    if old not in text:
        raise SystemExit(f"Expected upstream source marker not found in {path}: {old[:90]!r}")
    text = text.replace(old, new, 1)

replace_once(
    "#include <stdint.h>\n",
    "#include <stdint.h>\n#if defined(__SPU__)\n#include <spu_intrinsics.h>\n#endif\n"
)
replace_once(
    "static inline aa_vf aa_g(aa_vu p) { return __builtin_convertvector((aa_vi)((p >> 8) & 255u), aa_vf); }\n",
    """#if defined(__SPU__)
/* spu-gcc does not implement Clang's __builtin_convertvector. Use Cell SPU instructions. */
static inline aa_vf aa_to_float(aa_vi v) { return (aa_vf)spu_convtf((vector signed int)v, 0); }
static inline aa_vi aa_to_int(aa_vf v) { return (aa_vi)spu_convts((vector float)v, 0); }
#else
static inline aa_vf aa_to_float(aa_vi v) { return __builtin_convertvector(v, aa_vf); }
static inline aa_vi aa_to_int(aa_vf v) { return __builtin_convertvector(v, aa_vi); }
#endif

static inline aa_vf aa_g(aa_vu p) { return aa_to_float((aa_vi)((p >> 8) & 255u)); }
"""
)
replace_once(
    "    aa_vf a = __builtin_convertvector((aa_vi)((m >> shift) & 255u), aa_vf);\n"
    "    aa_vf b = __builtin_convertvector((aa_vi)((n >> shift) & 255u), aa_vf);\n",
    "    aa_vf a = aa_to_float((aa_vi)((m >> shift) & 255u));\n"
    "    aa_vf b = aa_to_float((aa_vi)((n >> shift) & 255u));\n"
)
replace_once(
    "        o = (pM & 0xff000000u) | ((aa_vu)__builtin_convertvector(rr, aa_vi) << 16)\n"
    "          | ((aa_vu)__builtin_convertvector(gg, aa_vi) << 8) | (aa_vu)__builtin_convertvector(bb, aa_vi);\n",
    "        o = (pM & 0xff000000u) | ((aa_vu)aa_to_int(rr) << 16)\n"
    "          | ((aa_vu)aa_to_int(gg) << 8) | (aa_vu)aa_to_int(bb);\n"
)
path.write_text(text)
print("Applied SPU vector conversion compatibility patch.")
