import re
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
PRX_DIR = PROJECT / "client" / "ps3" / "prx"
EXPORTS_YML = PRX_DIR / "exports.yml"
EXPORTS_ASM = PRX_DIR / "exports.S"


class PrxExportManifestTests(unittest.TestCase):
    def test_yaml_stub_manifest_matches_the_actual_prx_export_table(self):
        yaml_text = EXPORTS_YML.read_text(encoding="utf-8")
        asm_text = EXPORTS_ASM.read_text(encoding="utf-8")

        yaml_entries = re.findall(
            r"(?m)^  - name: ([A-Za-z_][A-Za-z0-9_]*)\n"
            r"    nid: '0x([0-9a-fA-F]{8})'\n"
            r"    signature: '[^']+'$",
            yaml_text,
        )
        asm_entries = re.findall(
            r"PRX_EXPORT_FUNC\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*"
            r"(0x[0-9a-fA-F]{8})\s*\)",
            asm_text,
        )
        yaml_map = {name: int(nid, 16) for name, nid in yaml_entries}
        asm_map = {name: int(nid, 16) for name, nid in asm_entries}

        self.assertEqual(len(yaml_entries), len(yaml_map), "duplicate names in exports.yml")
        self.assertEqual(len(asm_entries), len(asm_map), "duplicate names in exports.S")
        self.assertEqual(
            len({nid for _, nid in yaml_entries}),
            len(yaml_entries),
            "duplicate NIDs in exports.yml",
        )
        self.assertEqual(
            len({nid for _, nid in asm_entries}),
            len(asm_entries),
            "duplicate NIDs in exports.S",
        )
        self.assertEqual(yaml_map, asm_map)

    def test_read_only_script_override_diagnostics_are_exported(self):
        yaml_text = EXPORTS_YML.read_text(encoding="utf-8")
        asm_text = EXPORTS_ASM.read_text(encoding="utf-8")
        required = (
            "ghc_script_override_reload",
            "ghc_script_override_status",
            "ghc_script_override_comp_len",
            "ghc_script_override_stack_len",
            "ghc_script_override_bytecode_len",
            "ghc_script_override_total_len",
            "ghc_script_override_hash32",
            "ghc_script_override_blob_address",
            "ghc_script_override_stack_address",
            "ghc_script_override_bytecode_address",
        )
        for name in required:
            with self.subTest(export=name):
                self.assertIn(f"- name: {name}", yaml_text)
                self.assertIn(f"PRX_EXPORT_FUNC({name},", asm_text)


if __name__ == "__main__":
    unittest.main()
