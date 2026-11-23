// Export split-screen/player builtin string xrefs from one verified Ghosts PS3 PPU ELF.
// Run after importing the decrypted TU 1.16 ELF and completing Ghidra auto-analysis.
// Usage: FindGhostsSplitScreenBuiltins <output.csv>
// Read-only: this script does not patch the program.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class FindGhostsSplitScreenBuiltins extends GhidraScript {
    private static final String[] TERMS = {
        "isSplitScreen", "isSplitscreenPlayer", "isSplitscreenPlayerPrimary",
        "isPlayingSolo", "PlayerCMD_IsSplitScreenPlayer",
        "PlayerCMD_IsSplitScreenPlayerPrimary", "PlayerCMD_Spawn",
        "PlayerCMD_ClonePlayer", "SV_InitLocalClient",
        "CL_InitLocalClients", "CL_SetSplitScreen"
    };

    private String csv(String value) {
        if (value == null) value = "";
        String escaped = value.replace("\"", "\"\"");
        if (escaped.contains(",") || escaped.contains("\"") ||
            escaped.contains("\n") || escaped.contains("\r")) return "\"" + escaped + "\"";
        return escaped;
    }

    private boolean matches(byte[] data, int offset, byte[] pattern) {
        if (offset < 0 || offset + pattern.length > data.length) return false;
        for (int i = 0; i < pattern.length; i++) if (data[offset + i] != pattern[i]) return false;
        return true;
    }

    private void writeRow(BufferedWriter writer, String builtin, Address stringAddress, Reference reference) throws Exception {
        Address from = reference == null ? null : reference.getFromAddress();
        Function caller = from == null ? null : currentProgram.getFunctionManager().getFunctionContaining(from);
        Instruction instruction = from == null ? null : currentProgram.getListing().getInstructionAt(from);
        writer.write(csv(currentProgram.getName()) + "," + csv(builtin) + "," + csv(stringAddress.toString()) + ",");
        writer.write(csv(from == null ? "" : from.toString()) + ",");
        writer.write(csv(reference == null ? "" : reference.getReferenceType().toString()) + ",");
        writer.write(csv(reference == null ? "false" : Boolean.toString(reference.getReferenceType().isCall())) + ",");
        writer.write(csv(reference == null ? "false" : Boolean.toString(reference.getReferenceType().isData())) + ",");
        writer.write(csv(reference == null ? "" : reference.getSource().toString()) + ",");
        writer.write(csv(caller == null ? "" : caller.getName()) + ",");
        writer.write(csv(caller == null ? "" : caller.getEntryPoint().toString()) + ",");
        writer.write(csv(caller == null ? "" : caller.getBody().getMinAddress().toString()) + ",");
        writer.write(csv(caller == null ? "" : caller.getBody().getMaxAddress().toString()) + ",");
        writer.write(csv(instruction == null ? "" : instruction.toString()) + "\n");
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1 || args[0].trim().isEmpty()) {
            printerr("Usage: FindGhostsSplitScreenBuiltins <output.csv>");
            printerr("Run separately on the exact TU 1.16 Campaign and Multiplayer decrypted PPU ELFs.");
            return;
        }
        int stringHits = 0, referenceHits = 0, functionsResolved = 0;
        MemoryBlock[] blocks = currentProgram.getMemory().getBlocks();
        try (BufferedWriter writer = Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
            writer.write("program,builtin,string_address,reference_address,reference_type,is_call,is_data,source_type,caller_name,caller_entry,caller_body_start,caller_body_end,instruction\n");
            for (MemoryBlock block : blocks) {
                monitor.checkCancelled();
                if (!block.isInitialized()) continue;
                long size = block.getSize();
                if (size <= 0 || size > Integer.MAX_VALUE) {
                    println("Skipping unusually large block: " + block.getName() + " size=" + size);
                    continue;
                }
                byte[] bytes = new byte[(int)size];
                currentProgram.getMemory().getBytes(block.getStart(), bytes);
                for (String term : TERMS) {
                    byte[] pattern = term.getBytes(StandardCharsets.US_ASCII);
                    for (int offset = 0; offset <= bytes.length - pattern.length; offset++) {
                        if (!matches(bytes, offset, pattern)) continue;
                        Address stringAddress = block.getStart().add(offset);
                        stringHits++;
                        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(stringAddress);
                        int localRefs = 0;
                        while (refs.hasNext()) {
                            monitor.checkCancelled();
                            Reference reference = refs.next();
                            writeRow(writer, term, stringAddress, reference);
                            Function caller = currentProgram.getFunctionManager().getFunctionContaining(reference.getFromAddress());
                            if (caller != null) functionsResolved++;
                            localRefs++;
                            referenceHits++;
                        }
                        if (localRefs == 0) writeRow(writer, term, stringAddress, null);
                    }
                }
            }
        }
        println("Split-screen builtin xref CSV exported.");
        println("Program: " + currentProgram.getName());
        println("String hits: " + stringHits);
        println("Reference rows: " + referenceHits);
        println("References with containing functions: " + functionsResolved);
        println("Output: " + args[0]);
        println("Call and data reference types are exported separately; only explicit calls are call-path leads.");
        println("No result does not prove the builtin is absent; names may be hashed, stripped, or stored elsewhere.");
    }
}
