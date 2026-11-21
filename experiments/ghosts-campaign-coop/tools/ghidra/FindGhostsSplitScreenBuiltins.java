// Finds split-screen/player builtin strings and their Ghidra cross-references.
// Run after importing the user's decrypted Ghosts PS3 PPU ELF and completing
// Ghidra auto-analysis. This script is read-only; it does not patch the program.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.nio.charset.StandardCharsets;

public class FindGhostsSplitScreenBuiltins extends GhidraScript {
    private static final String[] TERMS = {
        "isSplitScreen",
        "isSplitscreenPlayer",
        "isSplitscreenPlayerPrimary",
        "isPlayingSolo",
        "PlayerCMD_IsSplitScreenPlayer",
        "PlayerCMD_IsSplitScreenPlayerPrimary",
        "PlayerCMD_Spawn",
        "PlayerCMD_ClonePlayer"
    };

    private boolean matches(byte[] data, int offset, byte[] pattern) {
        if (offset < 0 || offset + pattern.length > data.length) {
            return false;
        }
        for (int i = 0; i < pattern.length; i++) {
            if (data[offset + i] != pattern[i]) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void run() throws Exception {
        int stringHits = 0;
        int referenceHits = 0;
        MemoryBlock[] blocks = currentProgram.getMemory().getBlocks();

        for (MemoryBlock block : blocks) {
            monitor.checkCancelled();
            if (!block.isInitialized()) {
                continue;
            }
            long blockSize = block.getSize();
            if (blockSize <= 0 || blockSize > Integer.MAX_VALUE) {
                println("Skipping unusually large block: " + block.getName() + " size=" + blockSize);
                continue;
            }

            byte[] bytes = new byte[(int) blockSize];
            currentProgram.getMemory().getBytes(block.getStart(), bytes);

            for (String term : TERMS) {
                byte[] pattern = term.getBytes(StandardCharsets.US_ASCII);
                for (int offset = 0; offset <= bytes.length - pattern.length; offset++) {
                    if (!matches(bytes, offset, pattern)) {
                        continue;
                    }

                    Address stringAddress = block.getStart().add(offset);
                    stringHits++;
                    println("STRING " + term + " at " + stringAddress + " in " + block.getName());

                    ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(stringAddress);
                    int localRefs = 0;
                    while (refs.hasNext()) {
                        monitor.checkCancelled();
                        Reference ref = refs.next();
                        Address from = ref.getFromAddress();
                        Function containing = currentProgram.getFunctionManager().getFunctionContaining(from);
                        if (containing != null) {
                            println("  XREF " + from + " from " + containing.getName() + " @ " + containing.getEntryPoint());
                        } else {
                            println("  XREF " + from + " (no containing function yet)");
                        }
                        localRefs++;
                        referenceHits++;
                    }
                    if (localRefs == 0) {
                        println("  no references recorded; inspect nearby data/registration tables manually");
                    }
                }
            }
        }

        println("Split-screen builtin string scan complete.");
        println("String hits: " + stringHits);
        println("Recorded references: " + referenceHits);
        println("No result does not prove the builtin is absent; names may be hashed, stripped, or stored outside the loaded ELF.");
    }
}
