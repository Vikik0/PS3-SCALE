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

    private Map<String, OpdFunctionEvidence> collectOpdFunctionEvidence(
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

        Map<String, OpdFunctionEvidence> result = new TreeMap<>();
        for (long offset = 0; offset < opdSize; offset += 8L) {
            monitor.checkCancelled();
            Address descriptor = start.add(offset);
            int code = currentProgram.getMemory().getInt(descriptor);
            int toc = currentProgram.getMemory().getInt(descriptor.add(4L));
            long codeUnsigned = Integer.toUnsignedLong(code);
            long tocUnsigned = Integer.toUnsignedLong(toc);
            if (codeUnsigned == 0L && tocUnsigned == 0L) continue;
            if (codeUnsigned == 0L || (codeUnsigned & 3L) != 0L) continue;

            Address codeAddress = toAddr(codeUnsigned);
            MemoryBlock codeBlock = currentProgram.getMemory().getBlock(codeAddress);
            if (codeBlock == null || !codeBlock.isExecute()) continue;
            Function target = currentProgram.getFunctionManager().getFunctionAt(codeAddress);
            if (target == null) {
                target = currentProgram.getFunctionManager().getFunctionContaining(codeAddress);
            }
            if (target == null || target.isExternal()) continue;

            boolean tocMapped = tocUnsigned != 0L &&
                currentProgram.getMemory().contains(toAddr(tocUnsigned));
            String tocStatus = tocUnsigned == 0L ? "ZERO" : (tocMapped ? "MAPPED" : "UNMAPPED");
            String tocMapping = "0x" + String.format("%08X", tocUnsigned) + ":" + tocStatus;
            String functionEntry = target.getEntryPoint().toString();
            OpdFunctionEvidence evidence = result.get(functionEntry);
            if (evidence == null) {
                evidence = new OpdFunctionEvidence();
                result.put(functionEntry, evidence);
            }
            evidence.descriptorCount++;
            evidence.descriptorAddresses.add(descriptor.toString());
            evidence.tocMappings.add(tocMapping);
        }
        return result;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if ((args.length != 1 && args.length != 3) || args[0].trim().isEmpty()) {
            printerr("Usage: ExportGhostsPpuFunctionInventory <output.csv> [<opd-va> <opd-size>]");
            printerr("Run separately on the exact TU 1.16 Campaign and Multiplayer decrypted PPU ELFs.");
            return;
        }

        Map<String, OpdFunctionEvidence> opdEvidence = new TreeMap<>();
        if (args.length == 3) {
            opdEvidence = collectOpdFunctionEvidence(args[1], args[2]);
        }

        int exported = 0;
        int skipped = 0;
        try (BufferedWriter writer = Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
            writer.write("program,name,entry,body_address_count,instruction_count,fingerprint,call_targets,call_target_symbols,call_target_entries,opd_descriptor_addresses,opd_toc_mappings,opd_descriptor_count\n");

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
                OpdFunctionEvidence functionOpd = opdEvidence.get(function.getEntryPoint().toString());
                writer.write(csv(functionOpd == null ? "" : String.join(";", functionOpd.descriptorAddresses)));
                writer.write(",");
                writer.write(csv(functionOpd == null ? "" : String.join(";", functionOpd.tocMappings)));
                writer.write(",");
                writer.write(Integer.toString(functionOpd == null ? 0 : functionOpd.descriptorCount));
                writer.write("\n");
                exported++;
            }
        }

        println("Ghosts PPU function inventory exported.");
        println("Program: " + currentProgram.getName());
        println("Functions exported: " + exported);
        println("Functions skipped: " + skipped);
        int functionsWithOpd = 0;
        int mappedOpdDescriptors = 0;
        for (OpdFunctionEvidence evidence : opdEvidence.values()) {
            functionsWithOpd++;
            mappedOpdDescriptors += evidence.descriptorCount;
        }
        println("Functions with OPD descriptors: " + functionsWithOpd);
        println("OPD descriptors mapped to functions: " + mappedOpdDescriptors);
        println("Output: " + args[0]);
        println("Fingerprint normalization replaces long absolute addresses and auto-generated labels.");
        println("Matching fingerprints are navigation candidates only, not proof of identical semantics or safe hook targets.");
    }
}
