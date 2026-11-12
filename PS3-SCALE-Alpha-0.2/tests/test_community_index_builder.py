#!/usr/bin/env python3
"""Offline unit tests for the GitHub community index builder."""

import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "update_community_index.py"
SPEC = importlib.util.spec_from_file_location("community_index_builder", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


class CommunityIndexBuilderTests(unittest.TestCase):
    def test_named_game_and_version(self):
        self.assertEqual(
            MODULE.parse_ncl_name("Skate 3 BLUS30464 v01.05.ncl"),
            ("BLUS30464", "01.05", "Skate 3"),
        )

    def test_id_only_filename(self):
        title_id, version, title = MODULE.parse_ncl_name(
            "files/BCES00001_01.00.ncl"
        )
        self.assertEqual(title_id, "BCES00001")
        self.assertEqual(version, "01.00")
        self.assertEqual(title, "")

    def test_five_letter_classic_title_id(self):
        self.assertEqual(
            MODULE.parse_ncl_name(
                "Kirby Nightmare in Dream Land RXKIR1131 01.00.ncl"
            ),
            ("RXKIR1131", "01.00", "Kirby Nightmare in Dream Land"),
        )

    def test_multiple_versions_ignores_app_version(self):
        self.assertEqual(
            MODULE.parse_ncl_name(
                "God Of War Ascension BCUS98232 v01.00 av01.12.ncl"
            ),
            ("BCUS98232", "01.00", "God Of War Ascension"),
        )

    def test_patch_prefix_is_removed(self):
        title_id, version, title = MODULE.parse_ncl_name(
            "File 0x000CB720 Orig FBC1FFF0 - New 4E800020"
            "Metro Last Light Complete Edition BLES01999 01.00.ncl"
        )
        self.assertEqual(title_id, "BLES01999")
        self.assertEqual(version, "01.00")
        self.assertIn("Metro Last Light", title)

    def test_graphics_metadata_is_specific_and_conservative(self):
        content = (
            "Infinite Health\n0\nauthor\n0 00112233 00\n#\n"
            "Unlock FPS\n1\n0 00445566 6000\n#\n"
        )
        expected = (
            "FPS cap unlock; 60 FPS is not guaranteed. "
            "File also contains other game codes."
        )
        self.assertEqual(
            MODULE.graphics_description_from_ncl(content),
            expected,
        )
        self.assertEqual(
            MODULE.metadata_from_ncl_content("HARDWARE_TESTED_FPS", content),
            ("GRAPHICS_FPS", expected),
        )
        self.assertEqual(
            MODULE.metadata_from_ncl_content(
                "HARDWARE_TESTED_FPS", "Infinite Health\n0\nauthor\n"
            )[0],
            "COMMUNITY_CODE",
        )

    def test_cheats_are_not_classified_as_graphics(self):
        self.assertEqual(
            MODULE.metadata_for_file(
                "PS3-Pro/Game-Cheats", "main", "Skate 3.ncl",
                "COMMUNITY_CHEATS", 100,
            ),
            ("GAMEPLAY_CHEAT", "Gameplay cheat code; not a graphics enhancement."),
        )

    def test_source_trust_labels(self):
        self.assertEqual(
            MODULE.source_kind_for(
                "DoSpamu/RPCS3toArtemisPatches",
                "PSXPlace Confirmed/Skate 3.ncl",
                "COMMUNITY_UNVERIFIED",
            ),
            "COMMUNITY_CONFIRMED",
        )
        self.assertEqual(
            MODULE.source_kind_for(
                "DoSpamu/RPCS3toArtemisPatches",
                "USERLIST/Skate 3.ncl",
                "COMMUNITY_UNVERIFIED",
            ),
            "COMMUNITY_UNVERIFIED",
        )


if __name__ == "__main__":
    unittest.main()
