"""SIM3.3 sealed, distinct TRAIN/TEST selection model fixtures."""
import hashlib
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples/sim3"

try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator = None


def pinned(path):
    data = path.read_bytes()
    body, trailer = data.rsplit(b"seal ", 1)
    assert len(trailer) == 65 and trailer[-1:] == b"\n"
    assert hashlib.sha256(body).hexdigest().encode() == trailer[:-1]
    return body.decode(), hashlib.sha256(data).hexdigest()


class SelectionContracts(unittest.TestCase):
    def test_all_input_manifests_sealed_and_hash_pinned(self):
        body, _ = pinned(EXAMPLE / "model-search.cevselect")
        self.assertIn("domain SYNTHETIC_ONLY\n", body)
        self.assertIn("candidate baseline", body)
        self.assertIn("candidate fast-learning", body)
        self.assertIn("reserved_test_sha256 ", body)
        for line in body.splitlines():
            parts = line.split()
            if parts and parts[0] == "train":
                self.assertEqual(hashlib.sha256((ROOT / parts[1]).read_bytes()).hexdigest(), parts[2])
            if parts and parts[0] == "reserved_test_sha256":
                self.assertEqual(hashlib.sha256((EXAMPLE / "model-search-test.cevsplits").read_bytes()).hexdigest(), parts[1])
            if parts and parts[0] == "candidate":
                self.assertEqual(hashlib.sha256((ROOT / parts[2]).read_bytes()).hexdigest(), parts[3])
                self.assertEqual(hashlib.sha256((ROOT / parts[4]).read_bytes()).hexdigest(), parts[5])
                pinned(ROOT / parts[4])

    def test_train_and_test_have_disjoint_observation_keys(self):
        split_keys = {}
        for filename, partition in (("model-search-train.cevsplits", "TRAIN_ONLY"),
                                    ("model-search-test.cevsplits", "TEST_ONLY")):
            text, digest = pinned(EXAMPLE / filename)
            self.assertIn("partition " + partition + "\n", text)
            self.assertIn("origin SYNTHETIC_FIXTURE\n", text)
            keys = {(line.split()[1], line.split()[2], line.split()[3])
                    for line in text.splitlines() if line.startswith("row ")}
            self.assertEqual(len(keys), len([line for line in text.splitlines() if line.startswith("row ")]))
            split_keys[partition] = keys
            self.assertRegex(digest, re.compile("^[a-f0-9]{64}$"))
        self.assertFalse(split_keys["TRAIN_ONLY"] & split_keys["TEST_ONLY"])

    def test_untouched_test_report_and_locked_winner(self):
        body, selected_sha = pinned(EXAMPLE / "model-search-winner.sample.cevwinner")
        self.assertIn("winner baseline\n", body)
        report = json.loads((EXAMPLE / "model-search-final.sample.json").read_text())
        self.assertEqual(report["selection_sha256"], selected_sha)
        self.assertEqual(report["model"], "baseline")
        self.assertFalse(report["independent_holdout_verified"])
        self.assertEqual(report["test"]["count"], 16)
        self.assertEqual(len(report["rows"]), 16)
        self.assertTrue(all(r["tick"] >= 3 for r in report["rows"]))
        self.assertEqual(report["classification"], "SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION")
        if Draft202012Validator:
            schema = json.loads((ROOT / "schemas/software/model_selection_report.v1.schema.json").read_text())
            Draft202012Validator.check_schema(schema)
            validator = Draft202012Validator(schema)
            validator.validate(report)
            invalid = dict(report, independent_holdout_verified=True)
            self.assertFalse(validator.is_valid(invalid))
            invalid = dict(report, physical_stimulation="enabled")
            self.assertFalse(validator.is_valid(invalid))


if __name__ == "__main__":
    unittest.main()
