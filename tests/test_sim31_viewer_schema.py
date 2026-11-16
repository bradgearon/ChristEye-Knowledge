"""Schema validation of the synthetic SIM3.1 viewer demo and negative fixtures."""
import copy
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator = None


class Sim31ViewerSchema(unittest.TestCase):
    @unittest.skipIf(Draft202012Validator is None, "jsonschema not installed")
    def test_demo_and_negative_origins(self):
        schema = json.loads((ROOT / "schemas/software/colony_lab_frame.v1.schema.json").read_text())
        Draft202012Validator.check_schema(schema)
        validator = Draft202012Validator(schema)
        frames = [json.loads(line) for line in (ROOT / "tools/sim3_viewer/viewer-demo.ndjson").read_text().splitlines()]
        self.assertEqual(len(frames), 4)
        for frame in frames:
            validator.validate(frame)
        invalid = copy.deepcopy(frames[0])
        invalid["origin"] = "REAL_NEURAL_DATA"
        self.assertFalse(validator.is_valid(invalid))
        invalid = copy.deepcopy(frames[0])
        invalid["cells"][0]["energy"] = "unknown"
        self.assertFalse(validator.is_valid(invalid))
        invalid = copy.deepcopy(frames[0])
        invalid["physical_action"] = "STIMULATE"
        self.assertFalse(validator.is_valid(invalid))

    @unittest.skipIf(Draft202012Validator is None, "jsonschema not installed")
    def test_overlay_schema_is_valid(self):
        schema = json.loads((ROOT / "schemas/software/colony_lab_overlay.v1.schema.json").read_text())
        Draft202012Validator.check_schema(schema)
        validator = Draft202012Validator(schema)
        overlays = [json.loads(line) for line in (ROOT / "tools/sim3_viewer/overlay-demo.ndjson").read_text().splitlines()]
        self.assertEqual(len(overlays), 2)
        for overlay in overlays:
            validator.validate(overlay)
        broken = copy.deepcopy(overlays[0])
        broken["fields"]["layers"][0]["values"] = [0] * 12
        self.assertFalse(validator.is_valid(broken))


if __name__ == '__main__':
    unittest.main()
