"""CEVLM/2 visual catalog and viewer fixture checks (stdlib-only)."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SAMPLE = ROOT / "examples/vlm/process-garden.avlm"
FRAMES = ROOT / "examples/vlm/process-garden-frames.ndjson"
ALLOWED = (ROOT / "docs/assets/vlm").resolve()


class Vlm2VisualCatalogTests(unittest.TestCase):
    @staticmethod
    def catalog():
        assets, processes = {}, {}
        for line in SAMPLE.read_text(encoding="utf-8").splitlines():
            words = line.split()
            if not words or words[0].startswith("#"):
                continue
            if words[0] == "asset":
                assert len(words) == 5
                assets[words[1]] = dict(kind=words[2], path=words[3], sha256=words[4])
            if words[0] == "process":
                assert len(words) == 8
                processes[words[1]] = dict(operation=words[2], duration=int(words[3]),
                                           weight=int(words[4]), dx=int(words[5]),
                                           dy=int(words[6]), asset=words[7])
        return assets, processes

    def test_assets_are_real_pinned_safe_svg_files(self):
        assets, _ = self.catalog()
        self.assertEqual(len(assets), 5)
        for name, info in assets.items():
            with self.subTest(name=name):
                resolved = (ROOT / info["path"]).resolve()
                self.assertTrue(resolved.is_relative_to(ALLOWED))
                self.assertEqual(resolved.suffix, ".svg")
                self.assertTrue(resolved.is_file())
                self.assertEqual(hashlib.sha256(resolved.read_bytes()).hexdigest(), info["sha256"])
                svg = ET.parse(resolved).getroot()
                self.assertTrue(svg.tag.endswith("svg"))
                for element in svg.iter():
                    self.assertNotIn(element.tag.split("}")[-1].lower(), {"script", "foreignobject", "image"})
                    self.assertFalse(any(k.split("}")[-1].lower().startswith("on") for k in element.attrib))

    def test_process_contracts_reference_known_assets(self):
        assets, processes = self.catalog()
        self.assertEqual(len(processes), 5)
        for pid, p in processes.items():
            with self.subTest(process=pid):
                self.assertIn(p["asset"], assets)
                self.assertTrue(1 <= p["duration"] <= 512)
                self.assertTrue(1 <= p["weight"] <= 1000)
                self.assertTrue(-8 <= p["dx"] <= 8)
                self.assertTrue(-8 <= p["dy"] <= 8)

    def test_stream_fixture_is_simulated_and_time_ordered(self):
        assets, processes = self.catalog()
        frames = [json.loads(line) for line in FRAMES.read_text(encoding="utf-8").splitlines() if line]
        self.assertGreaterEqual(len(frames), 5)
        last_tick = None
        for frame in frames:
            with self.subTest(tick=frame["tick"]):
                self.assertEqual(frame["classification"], "SIMULATED_UNCALIBRATED")
                self.assertEqual(frame["format"], "CEVLM/2")
                if last_tick is not None:
                    self.assertEqual(frame["tick"], last_tick + 1)
                last_tick = frame["tick"]
                self.assertEqual(set(frame["field_tiles"]), {"support", "guidance", "rna", "protein", "soil", "waste"})
                self.assertTrue(all(len(field) == 8 and all(len(row) == 8 for row in field)
                                    for field in frame["field_tiles"].values()))
                for task in frame["processes"]:
                    self.assertIn(task["process"], processes)
                    self.assertEqual(task["asset_sha256"], assets[processes[task["process"]]["asset"]]["sha256"])
                    self.assertLessEqual(task["elapsed"], task["duration"])


if __name__ == "__main__":
    unittest.main()
