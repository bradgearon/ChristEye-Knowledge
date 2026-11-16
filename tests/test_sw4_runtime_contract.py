"""SW4.0 portable diagnostic schema tests; no physical care/actuation."""
from pathlib import Path
import json
import unittest
ROOT=Path(__file__).resolve().parents[1]
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator=None

class ColonyStateContractTest(unittest.TestCase):
    def test_fixture(self):
        schema=json.loads((ROOT/'schemas/software/colony_state_runtime.v1.schema.json').read_text())
        sample=json.loads((ROOT/'examples/sw4/latest-state.sample.json').read_text())
        self.assertEqual(sample['schema'],'anaiero.colony.latest/1')
        self.assertEqual(sample['origin'],'SIMULATED_UNCALIBRATED')
        self.assertEqual(sample['revision'],sample['updates'])
        self.assertGreater(len(sample['metrics']),0)
        self.assertLessEqual(len(sample['metrics']),256)
        self.assertEqual(sample['sequence_gaps'],0)
        if Draft202012Validator:
            Draft202012Validator.check_schema(schema)
            Draft202012Validator(schema).validate(sample)
            invalid=dict(sample);invalid['physical_feed_command']='nope'
            self.assertFalse(Draft202012Validator(schema).is_valid(invalid))
    def test_sources_are_not_silently_merged(self):
        sample=json.loads((ROOT/'examples/sw4/latest-state.sample.json').read_text())
        keys=[(m['colony'],m['animon'],m['source'],m['metric']) for m in sample['metrics']]
        self.assertEqual(len(keys),len(set(keys)))
        self.assertTrue(all(m['origin']=='SIMULATED' for m in sample['metrics']))

if __name__=='__main__':unittest.main()
