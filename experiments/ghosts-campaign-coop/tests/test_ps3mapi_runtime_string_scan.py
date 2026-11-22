import tempfile
import unittest
from pathlib import Path
from tools.ps3mapi.scan_runtime_strings import ELF_HEADER, PHDR, RuntimeElfError, scan
from tools.ps3mapi.compare_runtime_strings import compare
from tools.ps3mapi.diff_runtime_elfs import compare as diff_elfs

def make_elf(path: Path, strings: bytes, flags=5):
    offset = 0x10000
    image = bytearray(offset + len(strings))
    ident = b"\x7fELF" + bytes([2, 2, 1]) + bytes(9)
    header = ELF_HEADER.pack(
        ident, 2, 21, 1, 0x10000, 64, 0, 0,
        64, 56, 1, 0, 0, 0,
    )
    image[:len(header)] = header
    phdr = PHDR.pack(1, flags, offset, 0x10000, 0x10000, len(strings), len(strings), 0x10000)
    image[64:64 + len(phdr)] = phdr
    image[offset:offset + len(strings)] = strings
    path.write_bytes(image)

class RuntimeStringScanTests(unittest.TestCase):
    def test_scans_strings_and_reports_virtual_addresses(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "campaign.elf"
            make_elf(path, b"CL_CreateClient\x00isSplitScreen\x00camera_view\x00")
            result = scan(path)
            found = {item["string"]: item["locations"] for item in result["matches"]}
            self.assertIn("CL_CreateClient", found)
            self.assertEqual(found["CL_CreateClient"][0]["address"], "0x10000")
            self.assertIn("isSplitScreen", found)

    def test_compares_campaign_and_multiplayer_string_leads(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = root / "campaign.elf"
            multiplayer = root / "multiplayer.elf"
            make_elf(campaign, b"isSplitScreen\x00CL_CreateClient\x00")
            make_elf(multiplayer, b"isSplitScreen\x00localclient_alloc\x00")
            result = compare(campaign, multiplayer)
            common = {item["string"] for item in result["common"]}
            c_only = {item["string"] for item in result["campaign_only"]}
            m_only = {item["string"] for item in result["multiplayer_only"]}
            self.assertIn("isSplitScreen", common)
            self.assertIn("CL_CreateClient", c_only)
            self.assertIn("localclient_alloc", m_only)

    def test_binary_diff_reports_identical_and_changed_code_blocks(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = root / "campaign.elf"
            multiplayer = root / "multiplayer.elf"
            make_elf(campaign, b"A" * 0x1000)
            make_elf(multiplayer, b"A" * 0x800 + b"B" * 0x800)
            result = diff_elfs(campaign, multiplayer, block_size=0x100)
            self.assertEqual(result["totals"]["different_bytes"], 0x800)
            self.assertEqual(result["totals"]["identical_bytes"], 0x800)
            self.assertTrue(result["changed_blocks"])
            self.assertEqual(result["changed_blocks"][0]["virtual_address"], "0x10800")

    def test_binary_diff_can_include_non_executable_segments(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = root / "campaign.elf"
            multiplayer = root / "multiplayer.elf"
            make_elf(campaign, b"DATA-DATA", flags=6)
            make_elf(multiplayer, b"DATA-EDIT", flags=6)
            code_only = diff_elfs(campaign, multiplayer)
            all_segments = diff_elfs(campaign, multiplayer, executable_only=False)
            self.assertEqual(code_only["overlapping_segment_pairs"], [])
            self.assertEqual(len(all_segments["overlapping_segment_pairs"]), 1)

    def test_binary_diff_rejects_non_power_of_two_block_size(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = root / "campaign.elf"
            multiplayer = root / "multiplayer.elf"
            make_elf(campaign, b"A" * 32)
            make_elf(multiplayer, b"B" * 32)
            with self.assertRaises(ValueError):
                diff_elfs(campaign, multiplayer, block_size=0x180)

    def test_rejects_non_ppu_elf(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.elf"
            path.write_bytes(b"not an elf")
            with self.assertRaises(RuntimeElfError):
                scan(path)

if __name__ == "__main__":
    unittest.main()
