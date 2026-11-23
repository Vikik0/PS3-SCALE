// Seeds Ghidra functions from the PS3 PPU OPD table.
// Import the user's decrypted EBOOT ELF as PowerPC:BE:64:default first.
// Run with script arguments: <OPD_VA> <OPD_SIZE> (hex or decimal).
// This is a read-only analysis helper: it does not patch or export game bytes.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

public class SeedGhostsOpdFunctions extends GhidraScript {
    private static final long MAX_PPU_ADDRESS = 0xFFFFFFFFL;
    private static final long PPU_ADDRESS_SPACE = 0x100000000L;

    private long parseAddress(String value) {
        String trimmed = value.trim();
        if (trimmed.startsWith("0x") || trimmed.startsWith("0X")) {
            return Long.parseUnsignedLong(trimmed.substring(2), 16);
        }
        return Long.parseUnsignedLong(trimmed, 10);
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            println("Usage: SeedGhostsOpdFunctions <OPD_VA> <OPD_SIZE>");
            println("Get these values from tools/ps3_elf_inspect.py; do not guess them.");
            return;
        }

        long opdVa = parseAddress(args[0]);
        long opdSize = parseAddress(args[1]);
        if (opdVa < 0 || opdVa > MAX_PPU_ADDRESS) {
            printerr("OPD virtual address must fit the unsigned 32-bit PPU address space.");
            return;
        }
        if ((opdVa & 7L) != 0L) {
            printerr("OPD virtual address must be 8-byte aligned.");
            return;
        }
        if (opdSize < 8 || (opdSize & 7L) != 0L) {
            printerr("OPD size must be at least 8 and divisible by 8 bytes.");
            return;
        }
        if (opdSize > PPU_ADDRESS_SPACE - opdVa) {
            printerr("OPD range exceeds the unsigned 32-bit PPU address space.");
            return;
        }

        Address opdStart = toAddr(opdVa);
        Address opdEnd = toAddr(opdVa + opdSize - 1);
        if (!currentProgram.getMemory().contains(opdStart) ||
            !currentProgram.getMemory().contains(opdEnd)) {
            printerr("OPD range is not fully mapped in the current Ghidra program.");
            return;
        }

        int descriptors = 0;
        int validCodePointers = 0;
        int created = 0;
        int alreadyPresent = 0;
        int invalid = 0;

        for (long offset = 0; offset < opdSize; offset += 8) {
            monitor.checkCancelled();
            Address descriptor = toAddr(opdVa + offset);
            final int code;
            final int toc;
            try {
                // PS3 PPU OPD layout: big-endian u32 code address, u32 TOC.
                code = getInt(descriptor);
                toc = getInt(descriptor.add(4));
            } catch (Exception readError) {
                invalid++;
                continue;
            }

            if (code == 0 && toc == 0) {
                continue;
            }
            descriptors++;
            long codeUnsigned = Integer.toUnsignedLong(code);
            if ((codeUnsigned & 3L) != 0L) {
                invalid++;
                continue;
            }
            Address codeAddress;
            try {
                codeAddress = toAddr(codeUnsigned);
            } catch (Exception badAddress) {
                invalid++;
                continue;
            }

            MemoryBlock block = currentProgram.getMemory().getBlock(codeAddress);
            if (block == null || !block.isExecute()) {
                invalid++;
                continue;
            }
            validCodePointers++;

            Function existing = currentProgram.getFunctionManager().getFunctionAt(codeAddress);
            if (existing != null) {
                alreadyPresent++;
                continue;
            }

            try {
                disassemble(codeAddress);
                Function createdFunction = createFunction(codeAddress, null);
                if (createdFunction != null) {
                    created++;
                } else {
                    invalid++;
                }
            } catch (Exception createError) {
                invalid++;
                println("Could not create function at " + codeAddress + ": " + createError.getMessage());
            }
        }

        println("Ghosts PS3 PPU OPD seeding finished.");
        println("OPD descriptors visited: " + descriptors);
        println("Pointers into executable blocks: " + validCodePointers);
        println("New functions created: " + created);
        println("Functions already present: " + alreadyPresent);
        println("Invalid/unreadable entries: " + invalid);
        println("Note: OPD only enumerates address-taken functions; it is not a complete function list.");
    }
}
