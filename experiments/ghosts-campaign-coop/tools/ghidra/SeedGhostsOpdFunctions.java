// Seeds Ghidra functions from the PS3 PPU OPD table.
// Import the user's decrypted EBOOT ELF as PowerPC:BE:64:default first.
// Run with script arguments: <OPD_VA> <OPD_SIZE> <SUMMARY_JSON> (hex or decimal).
// This is a read-only analysis helper: it does not patch or export game bytes.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashSet;
import java.util.Set;

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
        if (args.length != 3) {
            printerr("Usage: SeedGhostsOpdFunctions <OPD_VA> <OPD_SIZE> <SUMMARY_JSON>");
            throw new IllegalArgumentException("OPD seeding requires a summary JSON output path.");
        }

        long opdVa = parseAddress(args[0]);
        long opdSize = parseAddress(args[1]);
        if (opdVa < 0 || opdVa > MAX_PPU_ADDRESS) {
            printerr("OPD virtual address must fit the unsigned 32-bit PPU address space.");
            throw new IllegalArgumentException("OPD virtual address is outside the PPU address space.");
        }
        if ((opdVa & 7L) != 0L) {
            printerr("OPD virtual address must be 8-byte aligned.");
            throw new IllegalArgumentException("OPD virtual address is not aligned.");
        }
        if (opdSize < 8 || (opdSize & 7L) != 0L) {
            printerr("OPD size must be at least 8 and divisible by 8 bytes.");
            throw new IllegalArgumentException("OPD size is invalid.");
        }
        if (opdSize > PPU_ADDRESS_SPACE - opdVa) {
            printerr("OPD range exceeds the unsigned 32-bit PPU address space.");
            throw new IllegalArgumentException("OPD range exceeds the PPU address space.");
        }

        Address opdStart = toAddr(opdVa);
        Address opdEnd = toAddr(opdVa + opdSize - 1);
        if (!currentProgram.getMemory().contains(opdStart) ||
            !currentProgram.getMemory().contains(opdEnd)) {
            printerr("OPD range is not fully mapped in the current Ghidra program.");
            throw new IllegalArgumentException("OPD range is not mapped in the Ghidra program.");
        }

        int descriptorSlotsScanned = 0;
        int emptyDescriptors = 0;
        int nonemptyDescriptors = 0;
        int unreadableDescriptors = 0;
        int validCodePointers = 0;
        int invalidCodePointers = 0;
        int created = 0;
        int alreadyPresent = 0;
        int failedToCreate = 0;
        int zeroTocDescriptors = 0;
        Set<Long> tocValues = new HashSet<>();
        Set<Long> mappedTocValues = new HashSet<>();
        Set<Long> unmappedTocValues = new HashSet<>();

        for (long offset = 0; offset < opdSize; offset += 8) {
            monitor.checkCancelled();
            descriptorSlotsScanned++;
            Address descriptor = toAddr(opdVa + offset);
            final int code;
            final int toc;
            try {
                // PS3 PPU OPD layout: big-endian u32 code address, u32 TOC.
                code = getInt(descriptor);
                toc = getInt(descriptor.add(4));
            } catch (Exception readError) {
                unreadableDescriptors++;
                continue;
            }

            if (code == 0 && toc == 0) {
                emptyDescriptors++;
                continue;
            }
            nonemptyDescriptors++;
            long tocUnsigned = Integer.toUnsignedLong(toc);
            if (tocUnsigned == 0L) {
                zeroTocDescriptors++;
            } else {
                tocValues.add(tocUnsigned);
                Address tocAddress = toAddr(tocUnsigned);
                if (currentProgram.getMemory().contains(tocAddress)) {
                    mappedTocValues.add(tocUnsigned);
                } else {
                    unmappedTocValues.add(tocUnsigned);
                }
            }
            long codeUnsigned = Integer.toUnsignedLong(code);
            if (codeUnsigned == 0L || (codeUnsigned & 3L) != 0L) {
                invalidCodePointers++;
                continue;
            }
            Address codeAddress;
            try {
                codeAddress = toAddr(codeUnsigned);
            } catch (Exception badAddress) {
                invalidCodePointers++;
                continue;
            }

            MemoryBlock block = currentProgram.getMemory().getBlock(codeAddress);
            if (block == null || !block.isExecute()) {
                invalidCodePointers++;
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
                    failedToCreate++;
                }
            } catch (Exception createError) {
                failedToCreate++;
                println("Could not create function at " + codeAddress + ": " + createError.getMessage());
            }
        }

        int seededFunctions = created + alreadyPresent;
        String status = seededFunctions > 0 ? "OPD_SEEDING_COMPLETED" : "OPD_SEEDING_EMPTY";
        String summary = "{\n" +
            "  \"schema_version\": 2,\n" +
            "  \"status\": \"" + status + "\",\n" +
            "  \"opd_va\": \"0x" + Long.toHexString(opdVa).toUpperCase() + "\",\n" +
            "  \"opd_size\": \"0x" + Long.toHexString(opdSize).toUpperCase() + "\",\n" +
            "  \"descriptor_slots_scanned\": " + descriptorSlotsScanned + ",\n" +
            "  \"empty_descriptors\": " + emptyDescriptors + ",\n" +
            "  \"nonempty_descriptors\": " + nonemptyDescriptors + ",\n" +
            "  \"unreadable_descriptors\": " + unreadableDescriptors + ",\n" +
            "  \"valid_code_pointers\": " + validCodePointers + ",\n" +
            "  \"invalid_code_pointers\": " + invalidCodePointers + ",\n" +
            "  \"functions_created\": " + created + ",\n" +
            "  \"functions_already_present\": " + alreadyPresent + ",\n" +
            "  \"functions_failed_to_create\": " + failedToCreate + ",\n" +
            "  \"distinct_toc_values\": " + tocValues.size() + ",\n" +
            "  \"mapped_toc_values\": " + mappedTocValues.size() + ",\n" +
            "  \"unmapped_toc_values\": " + unmappedTocValues.size() + ",\n" +
            "  \"zero_toc_descriptors\": " + zeroTocDescriptors + "\n" +
            "}\n";
        Files.write(Paths.get(args[2]), summary.getBytes(StandardCharsets.UTF_8));

        println("Ghosts PS3 PPU OPD seeding finished.");
        println("OPD descriptor slots scanned: " + descriptorSlotsScanned);
        println("Empty descriptors: " + emptyDescriptors);
        println("Non-empty descriptors: " + nonemptyDescriptors);
        println("Unreadable descriptors: " + unreadableDescriptors);
        println("Pointers into executable blocks: " + validCodePointers);
        println("Invalid code pointers: " + invalidCodePointers);
        println("New functions created: " + created);
        println("Functions already present: " + alreadyPresent);
        println("Functions failed to create: " + failedToCreate);
        println("Distinct TOC values: " + tocValues.size());
        println("Mapped TOC values: " + mappedTocValues.size());
        println("Unmapped TOC values: " + unmappedTocValues.size());
        println("Zero-TOC descriptors: " + zeroTocDescriptors);
        println("Summary: " + args[2]);
        println("Note: OPD only enumerates address-taken functions; it is not a complete function list.");

        if (validCodePointers == 0) {
            throw new IllegalStateException("OPD seeding found no code pointers in executable memory.");
        }
        if (seededFunctions == 0) {
            throw new IllegalStateException("OPD seeding found no functions that could be created or were already present.");
        }
    }
}
