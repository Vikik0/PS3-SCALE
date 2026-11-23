import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from gsc_callgraph_audit import audit_mission, clean_source_lines, function_ranges


class GscCallgraphAuditTests(unittest.TestCase):
    def test_comments_strings_and_block_comments_are_ignored(self):
        lines = [
            'main()', '{',
            '    // helper();',
            '    self iprintln("helper() is only text");',
            '    /* fake_call();',
            '       fake_thread(); */',
            '    helper();',
            '}',
        ]
        clean = clean_source_lines(lines)
        self.assertEqual(sum(line.count("helper(") for line in clean), 1)
        self.assertEqual(function_ranges(clean), [("main", 1, 8)])

    def test_function_ranges_ignore_control_blocks(self):
        lines = clean_source_lines([
            'main()', '{', '    if ( 1 )', '    {', '        helper();',
            '    }', '    while ( 0 )', '    {', '        helper();',
            '    }', '}', 'helper()', '{', '    return;', '}',
        ])
        self.assertEqual(function_ranges(lines), [("main", 1, 11), ("helper", 12, 15)])

    def test_direct_thread_namespace_and_pointer_edges(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "maps").mkdir()
            (root / "maps" / "main.gsc").write_text(
                'magicreloadwhenreachenemy()\n{\n'
                '    thread begin();\n'
                '    maps\\helper::step();\n'
                '}\n'
                'begin()\n{\n'
                '    animscripts\\shared::register_callback("x", ::callback);\n'
                '}\n', encoding="utf-8"
            )
            (root / "maps" / "helper.gsc").write_text(
                'step()\n{\n    return;\n}\n', encoding="utf-8"
            )
            (root / "maps" / "callbacks.gsc").write_text(
                'callback()\n{\n    return;\n}\n', encoding="utf-8"
            )
            report = audit_mission("test", root, target_names_override=["begin", "callback", "step"])
            begin = next(item for item in report["target_findings"] if item["function"] == "begin")
            self.assertTrue(begin["reachable_from_script_roots_without_callbacks"])
            callback = next(item for item in report["target_findings"] if item["function"] == "callback")
            self.assertEqual(callback["function_pointer_count"], 1)
            self.assertTrue(callback["reachable_from_script_roots_with_callbacks"])
            self.assertFalse(callback["reachable_from_script_roots_without_callbacks"])
            step = next(item for item in report["target_findings"] if item["function"] == "step")
            self.assertEqual(step["call_site_count"], 1)
            self.assertEqual(step["call_sites"][0]["kind"], "namespaced_call")
            self.assertEqual(step["call_sites"][0]["resolution"], "namespace_path_match")
            self.assertTrue(step["reachable_from_script_roots_without_callbacks"])

    def test_unreferenced_helper_is_not_marked_reachable(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "main.gsc").write_text(
                'magicreloadwhenreachenemy()\n{\n    return;\n}\n'
                'unused_player_helper()\n{\n    level.player giveweapon("weapon");\n}\n',
                encoding="utf-8"
            )
            report = audit_mission("test", root, target_names_override=["unused_player_helper"])
            target = next(item for item in report["target_findings"] if item["function"] == "unused_player_helper")
            self.assertEqual(target["call_site_count"], 0)
            self.assertFalse(target["reachable_from_script_roots_with_callbacks"])

    def test_ambiguous_unqualified_symbol_is_not_guessed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "a.gsc").write_text(
                'magicreloadwhenreachenemy()\n{\n    target();\n}\n', encoding="utf-8"
            )
            (root / "b.gsc").write_text('target()\n{\n return;\n}\n', encoding="utf-8")
            (root / "c.gsc").write_text('target()\n{\n return;\n}\n', encoding="utf-8")
            report = audit_mission("test", root)
            unresolved = [item for item in report["unresolved_or_ambiguous_references"] if item["symbol"] == "target"]
            self.assertEqual(len(unresolved), 1)
            self.assertEqual(unresolved[0]["resolution"], "ambiguous_unqualified_symbol")

    def test_missing_directory_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            missing = Path(temp) / "missing"
            with self.assertRaisesRegex(ValueError, "does not exist"):
                audit_mission("test", missing)


if __name__ == "__main__":
    unittest.main()
