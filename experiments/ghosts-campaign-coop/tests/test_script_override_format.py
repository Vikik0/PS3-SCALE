import re
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
PRX = PROJECT / "client" / "ps3" / "prx"


def function_body(source: str, name: str) -> str:
    match = re.search(rf"(?m)^[ \t]*{re.escape(name)}\s*\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"function definition not found: {name}")
    opening = source.index("{", match.start())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated C function: {name}")


class ScriptOverrideFormatIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.loader = (PRX / "script_override.c").read_text(encoding="utf-8")
        cls.parser = (PRX / "script_override_format.c").read_text(encoding="utf-8")
        cls.makefile = (PRX / "Makefile").read_text(encoding="utf-8")

    def test_ps3_loader_uses_the_same_portable_parser_as_host_tests(self):
        reload_body = function_body(self.loader, "ghc_script_override_reload")
        self.assertIn("ghc_script_override_inspect_blob(", reload_body)
        self.assertIn("GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE", self.loader)
        self.assertIn("blob_info.hash32", reload_body)
        self.assertNotIn("read_le32", self.loader)
        self.assertNotIn("fnv1a32", self.loader)

    def test_parser_checks_capacity_before_segment_arithmetic(self):
        body = function_body(self.parser, "ghc_script_override_inspect_blob")
        too_large = body.index("if ( bytes_read > capacity )")
        header = body.index("if ( capacity < GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE")
        comp_bound = body.index("comp_len > capacity - GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE")
        bytecode_bound = body.index("bytecode_len > capacity - GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE - comp_len")
        total_check = body.index("if ( total_len != bytes_read )")
        self.assertLess(too_large, header)
        self.assertLess(header, comp_bound)
        self.assertLess(comp_bound, bytecode_bound)
        self.assertLess(bytecode_bound, total_check)

    def test_prx_build_compiles_and_links_the_portable_parser(self):
        self.assertIn("script_override_format.c", self.makefile)
        self.assertIn("$(BUILD)/script_override_format.o", self.makefile)


if __name__ == "__main__":
    unittest.main()
