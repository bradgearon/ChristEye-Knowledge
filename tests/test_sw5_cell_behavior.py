"""Check the immutable illustrative profile and its digital-only CLI projection."""
from __future__ import annotations
import hashlib
import json
import pathlib
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator = None

class CellAtlasContracts(unittest.TestCase):
    @staticmethod
    def verify_seal(path: pathlib.Path) -> str:
        raw = path.read_bytes()
        body, seal = raw.rsplit(b'seal ', 1)
        assert len(seal) == 65 and seal.endswith(b'\n')
        assert hashlib.sha256(body).hexdigest().encode() == seal.strip()
        return hashlib.sha256(raw).hexdigest()

    def test_profile_references_source_record(self):
        ref = self.verify_seal(ROOT/'examples/cells/synthetic-observations.cevsource')
        atlas = ROOT/'examples/cells/crest-blood-neural.cevatlas'
        self.verify_seal(atlas)
        contents = atlas.read_text()
        self.assertIn('domain SYNTHETIC_ONLY', contents)
        self.assertIn('reference_sha256 '+ref, contents)
        self.assertEqual(sum(line.startswith('cell ') for line in contents.splitlines()), 9)
        self.assertIn('uncertainty UNCALIBRATED', (ROOT/'examples/cells/synthetic-observations.cevsource').read_text())

    def test_run_plan_is_pinned_and_digital_only(self):
        profile = self.verify_seal(ROOT/'examples/cells/crest-blood-neural.cevatlas')
        plan = ROOT/'examples/cells/reference-development.cevcellrun'
        self.verify_seal(plan)
        contents = plan.read_text()
        self.assertIn('domain SYNTHETIC_ONLY', contents)
        self.assertIn('atlas_sha256 '+profile, contents)
        self.assertEqual(sum(line.startswith('stage ') for line in contents.splitlines()), 3)

    def test_report_contract(self):
        schema=json.loads((ROOT/'schemas/software/cell_behavior_snapshot.v1.schema.json').read_text())
        report=json.loads((ROOT/'examples/cells/synthetic-run.sample.json').read_text())
        self.assertEqual(report['origin'],'SIMULATED_UNCALIBRATED')
        self.assertEqual(report['cells'],report['viable']+report['historical'])
        self.assertEqual(report['tick'],18)
        self.assertEqual(report['links'],1)
        if Draft202012Validator:
            Draft202012Validator.check_schema(schema)
            validator=Draft202012Validator(schema)
            self.assertFalse(list(validator.iter_errors(report)))
            altered=dict(report,origin='REAL_LAB_QUALIFIED')
            self.assertTrue(list(validator.iter_errors(altered)))
            altered=dict(report,physical_action='feed')
            self.assertTrue(list(validator.iter_errors(altered)))

if __name__=='__main__':
    unittest.main()
