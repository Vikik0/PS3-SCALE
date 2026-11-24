// Export a read-only function inventory from one verified Ghosts PS3 PPU ELF.
// Run once on Campaign and once on Multiplayer after Ghidra auto-analysis.
// Usage: ExportGhostsPpuFunctionInventory <output.csv>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;

public class ExportGhostsPpuFunctionInventory extends GhidraScript {
    private String csv(String value) {
        if (value == null) value = "";
        String escaped = value.replace("\"", "\"\"");
        if (escaped.contains(",") || escaped.contains("\"") ||
            escaped.contains("\n") || escaped.contains("\r")) {
            return "\"" + escaped + "\"";
        }
        return escaped;
    }

    private String normalizeOperand(String value) {
        if (value == null) return "";
        return value.toLowerCase()
            .replaceAll("0x[0-9a-f]{5,}", "ADDR")
            .replaceAll("lab_[0-9a-f]+", "LAB")
            .replaceAll("fun_[0-9a-f]+", "FUN");
    }

    private String sha256(String value) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] bytes = digest.digest(value.getBytes(StandardCharsets.UTF_8));
        StringBuilder out = new StringBuilder();
        for (byte item : bytes) out.append(String.format("%02x", item & 0xff));
        return out.toString();
    }

    private static class OpdFunctionEvidence {
        final TreeSet<String> descriptorAddresses = new TreeSet<>();
        final TreeSet<String> tocMappings = new TreeSet<>();
        int descriptorCount = 0;
        int exactEntryDescriptorCount = 0;
        int containingFunctionDescriptorCount = 0;
        final TreeMap<String, TocRegionEvidence> tocRegions = new TreeMap<>();
    }

    private static class TocRegionEvidence {
        final String address;
        final String status;
        final String blockName;
        final String permissions;
        final String blockOffset;
        final Boolean loaded;
        final Boolean initialized;
        final Boolean artificial;
        final Boolean external;
        final Boolean volatileBlock;

        TocRegionEvidence(
            String address, String status, String blockName,
            String permissions, String blockOffset,
            Boolean loaded, Boolean initialized, Boolean artificial,
            Boolean external, Boolean volatileBlock
        ) {
            this.address = address;
            this.status = status;
            this.blockName = blockName;
            this.permissions = permissions;
            this.blockOffset = blockOffset;
            this.loaded = loaded;
            this.initialized = initialized;
            this.artificial = artificial;
            this.external = external;
            this.volatileBlock = volatileBlock;
        }
    }

    private static class OpdFunctionCollection {
        final Map<String, OpdFunctionEvidence> functions = new TreeMap<>();
        int descriptorSlotsScanned = 0;
        int emptyDescriptors = 0;
        int nonemptyDescriptors = 0;
        int unreadableDescriptors = 0;
        int validExecutableCodePointers = 0;
        int invalidCodePointers = 0;
        int exactEntryMatches = 0;
        int containingFunctionMatches = 0;
        int unresolvedExecutablePointers = 0;
    }

    private long parseUnsigned32(String value, String label) {
        String token = value == null ? "" : value.trim();
        if (token.isEmpty()) throw new IllegalArgumentException(label + " is empty");
        long parsed;
        if (token.startsWith("0x") || token.startsWith("0X")) {
            parsed = Long.parseUnsignedLong(token.substring(2), 16);
        } else {
            parsed = Long.parseUnsignedLong(token, 10);
        }
        if (parsed < 0L || parsed > 0xffffffffL) {
            throw new IllegalArgumentException(label + " must fit unsigned 32-bit PPU address space");
        }
        return parsed;
    }

    private OpdFunctionCollection collectOpdFunctionEvidence(
        String addressText, String sizeText
    ) throws Exception {
        long opdVa = parseUnsigned32(addressText, "OPD virtual address");
        long opdSize = parseUnsigned32(sizeText, "OPD size");
        if ((opdVa & 7L) != 0L || opdSize < 8L || (opdSize & 7L) != 0L ||
            opdSize > 0x100000000L - opdVa) {
            throw new IllegalArgumentException("OPD range must be 8-byte aligned and fit PPU address space");
        }

        Address start = toAddr(opdVa);
        Address end = toAddr(opdVa + opdSize - 1L);
        if (!currentProgram.getMemory().contains(start) ||
            !currentProgram.getMemory().contains(end)) {
            throw new IllegalArgumentException("OPD range is not mapped in the imported PPU ELF");
        }

        OpdFunctionCollection result = new OpdFunctionCollection();
        for (long offset = 0; offset < opdSize; offset += 8L) {
            monitor.checkCancelled();
            result.descriptorSlotsScanned++;
            Address descriptor = start.add(offset);
            final int code;
            final int toc;
            try {
                code = currentProgram.getMemory().getInt(descriptor);
                toc = currentProgram.getMemory().getInt(descriptor.add(4L));
            } catch (Exception readError) {
                result.unreadableDescriptors++;
                continue;
            }

            long codeUnsigned = Integer.toUnsignedLong(code);
            long tocUnsigned = Integer.toUnsignedLong(toc);
            if (codeUnsigned == 0L && tocUnsigned == 0L) {
                result.emptyDescriptors++;
                continue;
            }
            result.nonemptyDescriptors++;

            if (codeUnsigned == 0L || (codeUnsigned & 3L) != 0L) {
                result.invalidCodePointers++;
                continue;
            }

            final Address codeAddress;
            try {
                codeAddress = toAddr(codeUnsigned);
            } catch (Exception badAddress) {
                result.invalidCodePointers++;
                continue;
            }
            MemoryBlock codeBlock = currentProgram.getMemory().getBlock(codeAddress);
            if (codeBlock == null || !codeBlock.isExecute()) {
                result.invalidCodePointers++;
                continue;
            }
            result.validExecutableCodePointers++;

            Function target = currentProgram.getFunctionManager().getFunctionAt(codeAddress);
            boolean exactEntry = target != null && !target.isExternal() &&
                target.getEntryPoint().equals(codeAddress);
            boolean containingFunction = false;
            if (!exactEntry) {
                target = currentProgram.getFunctionManager().getFunctionContaining(codeAddress);
                containingFunction = target != null && !target.isExternal();
                if (!containingFunction) target = null;
            }

            if (target == null) {
                result.unresolvedExecutablePointers++;
                continue;
            }

            Address tocAddress = tocUnsigned == 0L ? null : toAddr(tocUnsigned);
            MemoryBlock tocBlock = tocAddress == null
                ? null : currentProgram.getMemory().getBlock(tocAddress);
            boolean tocMapped = tocBlock != null;
            String tocStatus = tocUnsigned == 0L ? "ZERO" : (tocMapped ? "MAPPED" : "UNMAPPED");
            String tocAddressText = "0x" + String.format("%08X", tocUnsigned);
            String tocMapping = tocAddressText + ":" + tocStatus;
            String tocBlockName = tocBlock == null ? "" : tocBlock.getName();
            String tocPermissions = tocBlock == null ? "" :
                (tocBlock.isRead() ? "r" : "-") +
                (tocBlock.isWrite() ? "w" : "-") +
                (tocBlock.isExecute() ? "x" : "-");
            String tocBlockOffset = tocBlock == null ? "" :
                "0x" + String.format(
                    "%08X", tocAddress.getOffset() - tocBlock.getStart().getOffset()
                );
            Boolean tocLoaded = tocBlock == null ? null : Boolean.valueOf(tocBlock.isLoaded());
            Boolean tocInitialized = tocBlock == null ? null : Boolean.valueOf(tocBlock.isInitialized());
            Boolean tocArtificial = tocBlock == null ? null : Boolean.valueOf(tocBlock.isArtificial());
            Boolean tocExternal = tocBlock == null ? null : Boolean.valueOf(tocBlock.isExternalBlock());
            Boolean tocVolatile = tocBlock == null ? null : Boolean.valueOf(tocBlock.isVolatile());
            TocRegionEvidence tocRegion = new TocRegionEvidence(
                tocAddressText, tocStatus, tocBlockName, tocPermissions, tocBlockOffset,
                tocLoaded, tocInitialized, tocArtificial, tocExternal, tocVolatile
            );
            String functionEntry = target.getEntryPoint().toString();
            OpdFunctionEvidence evidence = result.functions.get(functionEntry);
            if (evidence == null) {
                evidence = new OpdFunctionEvidence();
                result.functions.put(functionEntry, evidence);
            }
            evidence.descriptorCount++;
            evidence.descriptorAddresses.add(descriptor.toString());
            evidence.tocMappings.add(tocMapping);
            evidence.tocRegions.put(tocAddressText, tocRegion);
            if (exactEntry) {
                evidence.exactEntryDescriptorCount++;
                result.exactEntryMatches++;
            } else if (containingFunction) {
                evidence.containingFunctionDescriptorCount++;
                result.containingFunctionMatches++;
            }
        }
        return result;
    }

    private String jsonEscape(String value) {
        StringBuilder escaped = new StringBuilder();
        for (int i = 0; i < value.length(); i++) {
            char ch = value.charAt(i);
            if (ch == '\\' || ch == '"') {
                escaped.append('\\').append(ch);
            } else if (ch == '\n') {
                escaped.append("\\n");
            } else if (ch == '\r') {
                escaped.append("\\r");
            } else if (ch == '\t') {
                escaped.append("\\t");
            } else if (ch < 0x20) {
                escaped.append(String.format("\\u%04X", (int)ch));
            } else {
                escaped.append(ch);
            }
        }
        return escaped.toString();
    }

    private String serializeTocRegions(OpdFunctionEvidence evidence) {
        StringBuilder json = new StringBuilder("[");
        boolean first = true;
        for (TocRegionEvidence region : evidence.tocRegions.values()) {
            if (!first) json.append(",");
            first = false;
            json.append("{");
            json.append("\"toc\":\"").append(jsonEscape(region.address)).append("\",");
            json.append("\"status\":\"").append(jsonEscape(region.status)).append("\",");
            json.append("\"block_name\":\"").append(jsonEscape(region.blockName)).append("\",");
            json.append("\"permissions\":\"").append(jsonEscape(region.permissions)).append("\",");
            json.append("\"block_offset\":\"").append(jsonEscape(region.blockOffset)).append("\",");
            json.append("\"loaded\":").append(region.loaded == null ? "null" : region.loaded.toString()).append(",");
            json.append("\"initialized\":").append(region.initialized == null ? "null" : region.initialized.toString()).append(",");
            json.append("\"artificial\":").append(region.artificial == null ? "null" : region.artificial.toString()).append(",");
            json.append("\"external\":").append(region.external == null ? "null" : region.external.toString()).append(",");
            json.append("\"volatile\":").append(region.volatileBlock == null ? "null" : region.volatileBlock.toString());
            json.append("}");
        }
        json.append("]");
        return json.toString();
    }

    private void writeOpdResolutionSummary(
        String outputPath, String addressText, String sizeText,
        OpdFunctionCollection result
    ) throws Exception {
        long opdVa = parseUnsigned32(addressText, "OPD virtual address");
        long opdSize = parseUnsigned32(sizeText, "OPD size");
        String summary = "{\n" +
            "  \"schema_version\": 1,\n" +
            "  \"status\": \"OPD_FUNCTION_RESOLUTION_COMPLETED\",\n" +
            "  \"opd_va\": \"0x" + String.format("%08X", opdVa) + "\",\n" +
            "  \"opd_size\": \"0x" + String.format("%08X", opdSize) + "\",\n" +
            "  \"descriptor_slots_scanned\": " + result.descriptorSlotsScanned + ",\n" +
            "  \"empty_descriptors\": " + result.emptyDescriptors + ",\n" +
            "  \"nonempty_descriptors\": " + result.nonemptyDescriptors + ",\n" +
            "  \"unreadable_descriptors\": " + result.unreadableDescriptors + ",\n" +
            "  \"valid_executable_code_pointers\": " + result.validExecutableCodePointers + ",\n" +
            "  \"invalid_code_pointers\": " + result.invalidCodePointers + ",\n" +
            "  \"exact_entry_matches\": " + result.exactEntryMatches + ",\n" +
            "  \"containing_function_matches\": " + result.containingFunctionMatches + ",\n" +
            "  \"unresolved_executable_pointers\": " + result.unresolvedExecutablePointers + ",\n" +
            "  \"functions_with_opd_descriptors\": " + result.functions.size() + "\n" +
            "}\n";
        try (BufferedWriter writer = Files.newBufferedWriter(
            Paths.get(outputPath), StandardCharsets.UTF_8
        )) {
            writer.write(summary);
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if ((args.length != 1 && args.length != 4) || args[0].trim().isEmpty()) {
            printerr("Usage: ExportGhostsPpuFunctionInventory <output.csv> [<opd-va> <opd-size> <resolution-summary.json>]");
            printerr("Run separately on the exact TU 1.16 Campaign and Multiplayer decrypted PPU ELFs.");
            return;
        }

        OpdFunctionCollection opdCollection = null;
        if (args.length == 4) {
            if (args[3].trim().isEmpty()) {
                throw new IllegalArgumentException("OPD resolution summary path is empty");
            }
            opdCollection = collectOpdFunctionEvidence(args[1], args[2]);
        }

        int exported = 0;
        int skipped = 0;
        try (BufferedWriter writer = Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
            writer.write("program,name,entry,body_address_count,instruction_count,fingerprint,call_targets,call_target_symbols,call_target_entries,opd_descriptor_addresses,opd_toc_mappings,opd_descriptor_count,opd_exact_entry_descriptor_count,opd_containing_function_descriptor_count,opd_toc_region_evidence\n");

            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                monitor.checkCancelled();
                Function function = functions.next();
                if (function.isExternal()) {
                    skipped++;
                    continue;
                }

                StringBuilder signature = new StringBuilder();
                TreeSet<String> callTargets = new TreeSet<>();
                TreeSet<String> callTargetSymbols = new TreeSet<>();
                TreeSet<String> callTargetEntries = new TreeSet<>();
                int instructionCount = 0;
                InstructionIterator instructions =
                    currentProgram.getListing().getInstructions(function.getBody(), true);

                while (instructions.hasNext()) {
                    monitor.checkCancelled();
                    Instruction instruction = instructions.next();
                    instructionCount++;
                    signature.append(instruction.getMnemonicString().toUpperCase());

                    for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
                        signature.append('|');
                        signature.append(normalizeOperand(
                            instruction.getDefaultOperandRepresentation(operand)
                        ));
                    }
                    signature.append('\n');

                    Reference[] references =
                        currentProgram.getReferenceManager().getReferencesFrom(instruction.getAddress());
                    for (Reference reference : references) {
                        if (reference.isCall()) {
                            Address target = reference.getToAddress();
                            if (target != null) {
                                callTargets.add(target.toString());
                                Function targetFunction =
                                    currentProgram.getFunctionManager().getFunctionContaining(target);
                                if (targetFunction == null) {
                                    targetFunction =
                                        currentProgram.getFunctionManager().getFunctionAt(target);
                                }
                                String targetName = targetFunction == null
                                    ? "UNRESOLVED"
                                    : targetFunction.getName();
                                callTargetSymbols.add(target.toString() + ":" + targetName);
                                if (targetFunction != null && !targetFunction.isExternal()) {
                                    callTargetEntries.add(
                                        targetFunction.getEntryPoint().toString()
                                    );
                                }
                            }
                        }
                    }
                }

                if (instructionCount == 0) {
                    skipped++;
                    continue;
                }

                writer.write(csv(currentProgram.getName()));
                writer.write(",");
                writer.write(csv(function.getName()));
                writer.write(",");
                writer.write(csv(function.getEntryPoint().toString()));
                writer.write(",");
                writer.write(Long.toString(function.getBody().getNumAddresses()));
                writer.write(",");
                writer.write(Integer.toString(instructionCount));
                writer.write(",");
                writer.write(sha256(signature.toString()));
                writer.write(",");
                writer.write(csv(String.join(";", callTargets)));
                writer.write(",");
                writer.write(csv(String.join(";", callTargetSymbols)));
                writer.write(",");
                writer.write(csv(String.join(";", callTargetEntries)));
                writer.write(",");
                OpdFunctionEvidence functionOpd = opdCollection == null
                    ? null : opdCollection.functions.get(function.getEntryPoint().toString());
                writer.write(csv(functionOpd == null ? "" : String.join(";", functionOpd.descriptorAddresses)));
                writer.write(",");
                writer.write(csv(functionOpd == null ? "" : String.join(";", functionOpd.tocMappings)));
                writer.write(",");
                writer.write(Integer.toString(functionOpd == null ? 0 : functionOpd.descriptorCount));
                writer.write(",");
                writer.write(Integer.toString(functionOpd == null ? 0 : functionOpd.exactEntryDescriptorCount));
                writer.write(",");
                writer.write(Integer.toString(functionOpd == null ? 0 : functionOpd.containingFunctionDescriptorCount));
                writer.write(",");
                writer.write(csv(functionOpd == null ? "[]" : serializeTocRegions(functionOpd)));
                writer.write("\n");
                exported++;
            }
        }

        if (opdCollection != null) {
            writeOpdResolutionSummary(args[3], args[1], args[2], opdCollection);
        }
        println("Ghosts PPU function inventory exported.");
        println("Program: " + currentProgram.getName());
        println("Functions exported: " + exported);
        println("Functions skipped: " + skipped);
        if (opdCollection != null) {
            int mappedOpdDescriptors = 0;
            for (OpdFunctionEvidence evidence : opdCollection.functions.values()) {
                mappedOpdDescriptors += evidence.descriptorCount;
            }
            println("Functions with OPD descriptors: " + opdCollection.functions.size());
            println("OPD descriptors mapped to functions: " + mappedOpdDescriptors);
            println("OPD exact-entry matches: " + opdCollection.exactEntryMatches);
            println("OPD containing-function matches: " + opdCollection.containingFunctionMatches);
            println("OPD unresolved executable pointers: " + opdCollection.unresolvedExecutablePointers);
            println("OPD resolution summary: " + args[3]);
        }
        println("Output: " + args[0]);
        println("Fingerprint normalization replaces long absolute addresses and auto-generated labels.");
        println("Matching fingerprints are navigation candidates only, not proof of identical semantics or safe hook targets.");
    }
}
