/*
 * Minimal CellOS LV2 end-file object required by the PS3DK SELF link path.
 *
 * The PS3SDK/PSL1GHT CRT uses this to close the .init and .fini sections.
 * Kept as a tiny source file so CI can build it for the pinned PPU toolchain
 * instead of assuming a prebuilt lv2-crtn.o exists in the downloaded archive.
 */
	.section ".init","ax"
	addi	r1,r1,128
	ld	r0,16(r1)
	mtlr	r0
	blr

	.section ".fini","ax"
	addi	r1,r1,128
	ld	r0,16(r1)
	mtlr	r0
	blr
