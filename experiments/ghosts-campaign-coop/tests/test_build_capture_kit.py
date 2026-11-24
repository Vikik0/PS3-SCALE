import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.build_capture_kit import PS3MAPI_FILES, build_capture_kit


def write_gsc_manifest(directory: Path, name: str, *, include_source=True):
    source = directory / f"{name}.gsc"
    binary = directory / f"{name}.gscbin"
    if include_source:
        source.write_text(f"{name}() {{ return 1; }}\n", encoding="utf-8")
    binary.write_bytes((name + "-compiled").encode("utf-8"))
    manifest = {
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "compiled_gscbin_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "compiled_gscbin_bytes": binary.stat().st_size,
        "status": "compiled but not injected or runtime-tested",
    }
    (directory / f"{name}.manifest.json").write_text(json.dumps(manifest), encoding="utf-8")


def make_project(root: Path):
    ps3mapi = root / "tools" / "ps3mapi"
    ps3mapi.mkdir(parents=True)
    for filename in PS3MAPI_FILES:
        (ps3mapi / filename).write_text(f"tool {filename}\n", encoding="utf-8")
    for filename in ("compare_pad_probe_logs.py", "compare_pad_input_probe_logs.py", "verify_capture_kit.py"):
        (root / "tools" / filename).write_text(f"# {filename}\n", encoding="utf-8")
    gsc = root / "client" / "ps3" / "gsc"
    gsc.mkdir(parents=True)
    (gsc / "ghc_campaign_player_registry.gsc").write_text(
        "ghc_campaign_player_registry_init() { return 1; }\n", encoding="utf-8"
    )


def make_artifacts(root: Path, project: Path):
    dirs = {name: root / name for name in ("pad-probe", "input-probe", "bootstrap", "session", "registry")}
    for path in dirs.values():
        path.mkdir(parents=True)

    for filename in ("ghosts_campaign_pad_probe.prx", "ghosts_campaign_pad_probe.fake.sprx", "ghosts_campaign_pad_probe.elf"):
        target = dirs["pad-probe"] / "build" / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(filename.encode())
    (dirs["pad-probe"] / "README_AR.md").write_text("pad probe readme", encoding="utf-8")
    modes = dirs["pad-probe"] / "mode_files"
    modes.mkdir()
    (modes / "ghosts_probe_mode_CAMPAIGN.txt").write_text("CAMPAIGN\n", encoding="utf-8")
    (modes / "ghosts_probe_mode_MULTIPLAYER.txt").write_text("MULTIPLAYER\n", encoding="utf-8")

    for filename in ("ghosts_pad_input_probe.prx", "ghosts_pad_input_probe.fake.sprx", "ghosts_pad_input_probe.elf"):
        target = dirs["input-probe"] / "build" / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(filename.encode())
    (dirs["input-probe"] / "README_AR.md").write_text("input probe readme", encoding="utf-8")

    for filename in ("ghosts_coop.prx", "ghosts_coop.fake.sprx", "ghosts_coop.elf", "libghosts_coop_stub.a"):
        target = dirs["bootstrap"] / "build" / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(filename.encode())

    session = dirs["session"]
    write_gsc_manifest(session, "ghc_campaign_local_session")

    registry = dirs["registry"]
    (registry / "ghc_campaign_player_registry.gscbin").write_bytes(b"registry-compiled")
    # The source lives in the checked-out project; pin its exact bytes here.
    source = project / "client" / "ps3" / "gsc" / "ghc_campaign_player_registry.gsc"
    manifest = {
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "compiled_gscbin_sha256": hashlib.sha256((registry / "ghc_campaign_player_registry.gscbin").read_bytes()).hexdigest(),
        "compiled_gscbin_bytes": (registry / "ghc_campaign_player_registry.gscbin").stat().st_size,
        "status": "compiled but not injected or runtime-tested",
    }
    (registry / "ghc_campaign_player_registry.manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    return dirs


class CaptureKitBuilderTests(unittest.TestCase):
    def test_builds_and_validates_a_reproducible_kit_from_ci_artifacts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            project = root / "project"
            artifacts = root / "artifacts"
            project.mkdir()
            artifacts.mkdir()
            make_project(project)
            make_artifacts(artifacts, project)
            output = root / "out" / "capture-kit.zip"

            result = build_capture_kit(project, artifacts, output, {"run_id": "test-run"})

            self.assertTrue(output.is_file())
            self.assertGreater(result["files_verified"], 0)
            self.assertEqual(result["sha256"], hashlib.sha256(output.read_bytes()).hexdigest())
            with zipfile.ZipFile(output) as archive:
                names = archive.namelist()
                self.assertTrue(any(name.endswith("/PS3/ghosts_coop/ghosts_coop.fake.sprx") for name in names))
                self.assertTrue(any(name.endswith("/PS3/ghosts_pad_input_probe.fake.sprx") for name in names))
                self.assertTrue(any(name.endswith("/Experimental_GSC_Not_Installed/ghc_campaign_local_session.gscbin") for name in names))
                self.assertFalse(any(name.startswith("Ghosts_TU116_Development_Capture_Kit/PS3/") and name.endswith(".gscbin") for name in names))

    def test_missing_ci_artifact_payload_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            project = root / "project"
            artifacts = root / "artifacts"
            project.mkdir()
            artifacts.mkdir()
            make_project(project)
            make_artifacts(artifacts, project)
            (artifacts / "input-probe" / "build" / "ghosts_pad_input_probe.fake.sprx").unlink()

            with self.assertRaises(FileNotFoundError):
                build_capture_kit(project, artifacts, root / "out.zip")


if __name__ == "__main__":
    unittest.main()
