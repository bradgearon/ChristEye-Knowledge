"""Portable CE-SW-09 fixture validation, independent of network or biology."""
from __future__ import annotations
import hashlib
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEX = re.compile(r"[0-9a-f]{64}\Z")


class TestLearningPlanFixtures(unittest.TestCase):
    def test_plan_seal_and_all_content_hashes(self):
        plan = (ROOT / "examples/vlm/bread-first.cevplan").read_text()
        preseal, seal = plan.rsplit("seal ", 1)
        self.assertEqual(hashlib.sha256(preseal.encode()).hexdigest(), seal.strip())
        for line in preseal.splitlines():
            parts = line.split()
            if parts and parts[0] == "artifact":
                self.assertEqual(len(parts), 5)
                path = ROOT / parts[3]
                self.assertTrue(path.is_file())
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), parts[4])

    def test_receipts_are_one_to_one_stage_maps_with_proxies(self):
        receipts = [json.loads(line) for line in (ROOT / "examples/vlm/bread-first-receipts.ndjson").read_text().splitlines()]
        self.assertEqual(len(receipts), 12)
        self.assertEqual([r["tick"] for r in receipts], list(range(1, 13)))
        self.assertEqual([r["stage"] for r in receipts if r["completed"]],
                         ["learn-symbol", "support-wave", "rna-wave"])
        for receipt in receipts:
            self.assertEqual(receipt["origin"], "SIMULATED_UNCALIBRATED")
            self.assertEqual(receipt["event"], "CE.PLAN.PROGRESS/1")
            for ref in ("receipt_sha256", "plan_sha256", "base_sha256", "crystal_sha256", "learning_sha256", "rna_sha256", "support_sha256", "visual_sha256"):
                self.assertRegex(receipt[ref], HEX)
            self.assertGreaterEqual(receipt["rna_field_proxy"], 0)
            self.assertGreaterEqual(receipt["support_field_proxy"], 0)
            self.assertGreaterEqual(receipt["learning_proxy"], 0)
            self.assertGreaterEqual(receipt["neurite_proxy"], 0)

    def test_saved_machine_has_reverse_mapping_sidecar(self):
        root = ROOT / "examples/vlm"
        lines = (root / "bread-first-trained.avlm.cevmap").read_text().splitlines()
        self.assertEqual(lines[0], "CEVMAP/1")
        content = "\n".join(lines[:-1]) + "\n"
        self.assertEqual(lines[-1], "seal " + hashlib.sha256(content.encode()).hexdigest())
        plan_content = (root / "bread-first.cevplan").read_text()
        self.assertIn("plan_sha256 " + plan_content.rsplit("seal ", 1)[1].strip(), content)
        receipts = (root / "bread-first-receipts.ndjson").read_bytes()
        self.assertIn("receipts_sha256 " + hashlib.sha256(receipts).hexdigest(), content)
        self.assertEqual(len([l for l in lines if l.startswith("binding ")]), 3)

    def test_example_documents_do_not_claim_biological_knowledge_transfer(self):
        documents = ["digital-crystal-demo.txt", "lesson-demo.txt", "rna-observation-demo.txt", "support-profile-demo.txt"]
        for name in documents:
            with self.subTest(name=name):
                text = (ROOT / "examples/vlm/data" / name).read_text()
                self.assertIn("SIMULATED_UNCALIBRATED", text)
                self.assertFalse("STIMULATE_NOW" in text)


if __name__ == "__main__":
    unittest.main()
