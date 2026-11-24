import hashlib
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from tools import verify_ghosts_mission_fastfiles as verifier


class VerifyGhostsMissionFastFilesTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.data = {
            "black_ice": b"IWff0100" + bytes.fromhex("0000022E") + b"black-ice-test",
            "carrier": b"IWff0100" + bytes.fromhex("0000022E") + b"carrier-test",
            "cornered": b"IWff0100" + bytes.fromhex("0000022E") + b"cornered-test",
        }
        self.paths = {}
        self.expectations = {}
        for role, data in self.data.items():
            path = self.root / f"{role}.ff"
            path.write_bytes(data)
            self.paths[role] = path
            self.expectations[role] = {
                "file_name": path.name,
                "size_bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
                "magic": "IWff0100",
                "version": "0x0000022E",
            }

    def tearDown(self):
        self.temp.cleanup()

    def test_accepts_three_exact_fastfile_inputs(self):
        with patch.dict(verifier.EXPECTED_FASTFILES, self.expectations, clear=True):
            report = verifier.verify_fastfiles(self.paths)
        self.assertEqual(report["status"], "MATCHED_MISSION_FASTFILES")
        self.assertEqual(
            {role: item["status"] for role, item in report["files"].items()},
            {"black_ice": "MATCHED", "carrier": "MATCHED", "cornered": "MATCHED"},
        )

    def test_rejects_wrong_hash_even_when_magic_and_size_match(self):
        changed = self.root / "changed-carrier.ff"
        changed.write_bytes(b"IWff0100" + b"changed-test")
        expectations = dict(self.expectations)
        expectations["carrier"] = {
            **self.expectations["carrier"],
            "size_bytes": changed.stat().st_size,
        }
        paths = {**self.paths, "carrier": changed}
        with patch.dict(verifier.EXPECTED_FASTFILES, expectations, clear=True):
            report = verifier.verify_fastfiles(paths)
        self.assertEqual(report["status"], "MISMATCH_OR_INCOMPLETE")
        self.assertFalse(report["files"]["carrier"]["checks"]["sha256"])
        self.assertTrue(report["files"]["carrier"]["checks"]["magic"])

    def test_reports_missing_role_without_throwing(self):
        paths = {"black_ice": self.paths["black_ice"], "carrier": self.paths["carrier"]}
        with patch.dict(verifier.EXPECTED_FASTFILES, self.expectations, clear=True):
            report = verifier.verify_fastfiles(paths)
        self.assertEqual(report["status"], "MISMATCH_OR_INCOMPLETE")
        self.assertEqual(report["files"]["cornered"]["status"], "MISSING")
        self.assertTrue(any("cornered: input path was not provided" in item for item in report["errors"]))

    def test_reports_missing_file_path(self):
        paths = {**self.paths, "cornered": self.root / "missing.ff"}
        with patch.dict(verifier.EXPECTED_FASTFILES, self.expectations, clear=True):
            report = verifier.verify_fastfiles(paths)
        self.assertEqual(report["files"]["cornered"]["status"], "MISSING")
        self.assertEqual(report["status"], "MISMATCH_OR_INCOMPLETE")



    def test_rejects_file_modified_during_single_pass_hash(self):
        path = self.paths["black_ice"]
        before = path.stat()
        changed = SimpleNamespace(
            st_dev=before.st_dev,
            st_ino=before.st_ino,
            st_size=before.st_size,
            st_mtime_ns=before.st_mtime_ns + 1,
            st_ctime_ns=before.st_ctime_ns,
        )
        with patch.object(verifier.os, "fstat", side_effect=[before, changed]):
            with self.assertRaisesRegex(OSError, "changed while being read"):
                verifier._read_identity(path)

    def test_rejects_path_replacement_during_single_pass_hash(self):
        path = self.paths["black_ice"]
        opened = path.stat()
        replaced = SimpleNamespace(
            st_dev=opened.st_dev,
            st_ino=opened.st_ino + 1,
            st_size=opened.st_size,
            st_mtime_ns=opened.st_mtime_ns,
            st_ctime_ns=opened.st_ctime_ns,
        )
        with patch.object(verifier.os, "fstat", side_effect=[opened, opened]), patch.object(
            Path, "stat", return_value=replaced
        ):
            with self.assertRaisesRegex(OSError, "changed while being read"):
                verifier._read_identity(path)

    def test_rejects_wrong_version_even_when_hash_and_size_match(self):
        wrong_version = self.root / "wrong-version-carrier.ff"
        data = b"IWff0100" + bytes.fromhex("0000022F") + b"carrier-test"
        wrong_version.write_bytes(data)
        expectations = dict(self.expectations)
        expectations["carrier"] = {
            **self.expectations["carrier"],
            "size_bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        }
        paths = {**self.paths, "carrier": wrong_version}
        with patch.dict(verifier.EXPECTED_FASTFILES, expectations, clear=True):
            report = verifier.verify_fastfiles(paths)
        self.assertEqual(report["status"], "MISMATCH_OR_INCOMPLETE")
        self.assertTrue(report["files"]["carrier"]["checks"]["size_bytes"])
        self.assertTrue(report["files"]["carrier"]["checks"]["sha256"])
        self.assertTrue(report["files"]["carrier"]["checks"]["magic"])
        self.assertFalse(report["files"]["carrier"]["checks"]["version"])
        self.assertEqual(report["files"]["carrier"]["version"], "0x0000022F")

if __name__ == "__main__":
    unittest.main()
