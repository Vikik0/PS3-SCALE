// Export a read-only function inventory from one verified Ghosts PS3 PPU ELF.
// Run once on Campaign and once on Multiplayer after Ghidra auto-analysis.
// Usage: ExportGhostsPpuFunctionInventory <output.csv>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
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

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1 || args[0].trim().isEmpty()) {
            printerr("Usage: ExportGhostsPpuFunctionInventory <output.csv>");
            printerr("Run separately on the exact TU 1.16 Campaign and Multiplayer decrypted PPU ELFs.");
            return;
        }

        int exported = 0;
        int skipped = 0;
        try (BufferedWriter writer = Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
            writer.write("program,name,entry,body_address_count,instruction_count,fingerprint,call_targets,call_target_symbols,call_target_entries\n");

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
                writer.write("\n");
                exported++;
            }
        }

        println("Ghosts PPU function inventory exported.");
        println("Program: " + currentProgram.getName());
        println("Functions exported: " + exported);
        println("Functions skipped: " + skipped);
        println("Output: " + args[0]);
        println("Fingerprint normalization replaces long absolute addresses and auto-generated labels.");
        println("Matching fingerprints are navigation candidates only, not proof of identical semantics or safe hook targets.");
    }
}
