"""CE-SW-10 diagnostic event contract tests; does not verify signatures or biology."""
import copy
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator=None

class WindRegistrySchemaTest(unittest.TestCase):
    def test_schema_and_sample(self):
        schema=json.loads((ROOT/'schemas/software/wind_registry_event.v1.schema.json').read_text())
        sample=json.loads((ROOT/'examples/vlm/registry-event.sample.json').read_text())
        self.assertEqual(schema['$schema'],'https://json-schema.org/draft/2020-12/schema')
        self.assertTrue(set(['producer','origin','event_sha256','previous_sha256']).issubset(schema['required']))
        self.assertEqual(sample['origin'],'SIMULATED')
        if Draft202012Validator is not None:
            Draft202012Validator.check_schema(schema)
            validator=Draft202012Validator(schema)
            validator.validate(sample)
            bad=copy.deepcopy(sample)
            bad['origin']='QUALIFIED_LAB'
            self.assertFalse(validator.is_valid(bad))
            bad=copy.deepcopy(sample)
            bad['object_sha256']='not-a-digest'
            self.assertFalse(validator.is_valid(bad))
            bad=copy.deepcopy(sample)
            bad['physical_actuation']='DO_NOT_ALLOW'
            self.assertFalse(validator.is_valid(bad))

if __name__=='__main__':
    unittest.main()
