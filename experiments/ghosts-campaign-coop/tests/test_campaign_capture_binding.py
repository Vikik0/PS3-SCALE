import hashlib
import tempfile
import unittest
from pathlib import Path

from tools.ps3mapi.campaign_capture_binding import bind_capture_to_campaign_self


class CampaignCaptureBindingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.campaign_self = self.root / "default.self"
        self.campaign_self.write_bytes(b"exact-campaign-self-fixture")
        self.digest = hashlib.sha256(self.campaign_self.read_bytes()).hexdigest()

    def tearDown(self):
        self.temp.cleanup()

    def test_matching_hash_binds_capture_to_exact_campaign_self(self):
        result = bind_capture_to_campaign_self(
            {"campaign_self_sha256": self.digest.upper()},
            self.campaign_self,
        )
        self.assertEqual(result["status"], "PASS_CAMPAIGN_SELF_SHA256_MATCH")
        self.assertTrue(result["capture_bound_to_analyzer_self"])
        self.assertEqual(result["campaign_self_sha256"], self.digest)
        self.assertEqual(result["capture_declared_campaign_self_sha256"], self.digest)

    def test_missing_hash_is_reported_as_unbound_not_authenticated(self):
        result = bind_capture_to_campaign_self({}, self.campaign_self)
        self.assertEqual(result["status"], "NOT_PROVIDED")
        self.assertFalse(result["capture_bound_to_analyzer_self"])
        self.assertEqual(result["campaign_self_sha256"], self.digest)
        self.assertIsNone(result["capture_declared_campaign_self_sha256"])

    def test_mismatched_hash_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "does not match"):
            bind_capture_to_campaign_self(
                {"campaign_self_sha256": "0" * 64},
                self.campaign_self,
            )

    def test_malformed_hash_is_rejected(self):
        for value in ("", "abc", "g" * 64, 123):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "64-character hexadecimal"):
                    bind_capture_to_campaign_self(
                        {"campaign_self_sha256": value},
                        self.campaign_self,
                    )

    def test_non_object_capture_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "JSON object"):
            bind_capture_to_campaign_self([], self.campaign_self)


if __name__ == "__main__":
    unittest.main()
