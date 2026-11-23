# Phase 27 — PPU call-graph comparison

## Purpose

Phase 26 exported resolved call-target symbols but the comparer only used the names of outgoing calls. An opaque Multiplayer-only function could still be missed when its own name and its direct callees were stripped or generic, even if a meaningful local-client routine called it.

Phase 27 adds resolved internal function-entry edges to the inventory and compares the Campaign and Multiplayer call graphs by function fingerprint.

## Changes

- The Ghidra exporter resolves each call reference to the function containing its target address and writes that function's entry point into the optional `call_target_entries` CSV column.
- External or unresolved targets remain outside the internal call graph. Their raw addresses and best-effort symbols remain in the existing columns.
- The Python reader remains backward compatible with inventories that do not contain `call_target_entries`.
- The comparer builds per-build internal call graphs using exact entry addresses from the same inventory.
- For matching function fingerprints, it reports callee-fingerprint changes between Campaign and Multiplayer.
- For Multiplayer-only candidates, it reports incoming callers, outgoing callee names/entries/fingerprints, callees whose fingerprints also occur in Campaign, and Multiplayer-only callees.
- The reader rejects duplicate function entry addresses so one call-graph node cannot silently overwrite another.
- Incoming callers' own outgoing symbol matches are labelled as caller context; they are not presented as the symbol that directly called the candidate.
- Incoming caller names and their target symbols contribute to candidate ranking. This can surface a generically named function called by a meaningful `SV_InitLocalClient`-style routine.
- The summary reports resolved internal edge counts and the number of matching caller fingerprints with changed callee sets.

## Operator workflow

Use the Phase 26 workflow to generate a Campaign and Multiplayer CSV from the exact TU 1.16 reconstructed PPU images. The exporter now includes `call_target_entries` automatically; no extra Ghidra script arguments are required.

Run the comparer as before:

~~~powershell
python tools/compare_ppu_function_inventories.py --campaign "D:\Ghosts\analysis\campaign_ppu_inventory.csv" --multiplayer "D:\Ghosts\analysis\multiplayer_ppu_inventory.csv" --output "D:\Ghosts\analysis\ppu_function_comparison.json"
~~~

Inspect these report sections in order:

1. `summary`: inventory sizes, resolved internal call-edge counts, and changed call-graph groups.
2. `call_graph_deltas`: matching function fingerprints whose resolved callee-fingerprint sets differ between builds.
3. `priority_multiplayer_only`: candidate functions, incoming callers, direct symbol hits, outgoing target fingerprints, and whether each outgoing target also exists in Campaign.
4. `call_graph_analysis.limitations`: known blind spots in the static graph.

## Validation boundary

A static call reference is not proof that the call executes during a particular mode or lifecycle stage. Indirect calls, tail calls, unresolved references, stripped function boundaries, and Ghidra analysis errors may hide edges. A function called by a local-client routine is only a lead; it is not automatically a spawn routine or a safe Campaign hook.

No user-owned decrypted PPU images or actual Ghidra inventories were supplied for this phase. Therefore the new graph logic is tested with synthetic fixtures only. No native Player 2 function, hook address, or runtime behavior has been identified or approved.
