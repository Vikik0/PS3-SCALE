import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.verify_capture_kit import validate_package


REQUIRED = (
    "PS3/ghosts_campaign_pad_probe.fake.sprx",
    "PS3/ghosts_pad_input_probe.fake.sprx",
    "PS3/ghosts_coop/ghosts_coop.fake.sprx",
    "Experimental_GSC_Not_Installed/ghc_campaign_player_registry.gscbin",
    "Experimental_GSC_Not_Installed/ghc_campaign_local_session.gscbin",
    "PS3/dump_campaign_tu116.bat",
    "PS3/dump_multiplayer_tu116.bat",
)


def write_package(root: Path):
    for relative in REQUIRED:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(("payload:" + relative).encode("utf-8"))

    for name in ("ghc_campaign_player_registry", "ghc_campaign_local_session"):
        base = root / "Experimental_GSC_Not_Installed"
        source = base / f"{name}.gsc"
        binary = base / f"{name}.gscbin"
        source.write_text(f"{name}() {{ return 1; }}\n", encoding="utf-8")
        binary.write_bytes((name + "-compiled").encode("utf-8"))
        manifest = {
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "compiled_gscbin_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "compiled_gscbin_bytes": binary.stat().st_size,
            "status": "compiled but not injected or runtime-tested",
        }
        (base / f"{name}.manifest.json").write_text(json.dumps(manifest), encoding="utf-8")

    files = {}
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "MANIFEST.json"):
        files[str(path.relative_to(root)).replace("\\", "/")] = {
            "size_bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
    manifest = {
        "package": "test kit",
        "target": {"title_id": "BLUS31270", "app_ver": "01.16"},
        "scope": "read-only kit; not a playable co-op mod",
        "files": files,
    }
    (root / "MANIFEST.json").write_text(json.dumps(manifest), encoding="utf-8")


class CaptureKitVerifierTests(unittest.TestCase):
    def test_valid_package_passes_all_hashes_and_gsc_pins(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            errors, summary = validate_package(root)
        self.assertEqual(errors, [])
        self.assertEqual(summary["files_declared"], summary["files_verified"])
        self.assertEqual(summary["gsc_manifests_verified"], 2)

    def test_modified_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            (root / REQUIRED[0]).write_bytes(b"tampered")
            errors, _ = validate_package(root)
        self.assertTrue(any("size mismatch" in error or "SHA-256 mismatch" in error for error in errors))

    def test_missing_required_probe_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            (root / "PS3/ghosts_pad_input_probe.fake.sprx").unlink()
            errors, _ = validate_package(root)
        self.assertTrue(any("required package file missing" in error for error in errors))

    def test_manifest_path_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            manifest_path = root / "MANIFEST.json"
            manifest = json.loads(manifest_path.read_text())
            manifest["files"]["../outside.bin"] = {"size_bytes": 1, "sha256": "0" * 64}
            manifest_path.write_text(json.dumps(manifest))
            errors, _ = validate_package(root)
        self.assertTrue(any("unsafe manifest path" in error for error in errors))

    def test_wrong_title_or_tu_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            manifest_path = root / "MANIFEST.json"
            manifest = json.loads(manifest_path.read_text())
            manifest["target"]["app_ver"] = "01.15"
            manifest_path.write_text(json.dumps(manifest))
            errors, _ = validate_package(root)
        self.assertIn("package target must be BLUS31270 / TU 01.16", errors)

    def test_gsc_source_hash_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            source = root / "Experimental_GSC_Not_Installed/ghc_campaign_local_session.gsc"
            source.write_text("modified", encoding="utf-8")
            errors, _ = validate_package(root)
        self.assertTrue(any("source SHA-256 mismatch" in error for error in errors))

    def test_gscbin_must_not_be_placed_in_ps3_payload_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_package(root)
            rogue = root / "PS3/ghc_campaign_local_session.gscbin"
            rogue.write_bytes(b"wrong location")
            errors, _ = validate_package(root)
        self.assertTrue(any("GSCBIN must remain under Experimental_GSC_Not_Installed" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
