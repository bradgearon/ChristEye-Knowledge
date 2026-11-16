"""SIM3.2 fixed synthetic data / report / schema checks. No biological validation."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator=None

class FidelityContractTests(unittest.TestCase):
    def sealed(self, path: Path) -> bytes:
        content=path.read_bytes()
        start=content.rfind(b"seal ")
        self.assertGreater(start,0)
        self.assertEqual(content[start:].strip(), b"seal "+hashlib.sha256(content[:start]).hexdigest().encode())
        self.assertEqual(content[-1:],b"\n")
        return content
    def test_profile_and_dataset_hashes(self):
        base=ROOT/"examples/sim3"
        law=self.sealed(base/"baseline.celaw")
        obs=self.sealed(base/"synthetic-reference.cevobs")
        manifest=self.sealed(base/"baseline.ceveval").decode()
        self.assertIn(hashlib.sha256(law).hexdigest(),manifest)
        self.assertIn(hashlib.sha256(obs).hexdigest(),manifest)
        self.assertIn(hashlib.sha256((ROOT/"examples/connected-gardens.cev").read_bytes()).hexdigest(),manifest)
        rows=[l.split() for l in obs.decode().splitlines() if l.startswith("row ")]
        self.assertEqual(len(rows),28)
        self.assertEqual(sum(r[-1]=="train" for r in rows),12)
        self.assertEqual(sum(r[-1]=="holdout" for r in rows),16)
        self.assertEqual(len({(r[1],r[2],r[3]) for r in rows}),len(rows))
    def test_report_schema_and_synthetic_metrics(self):
        report=json.loads((ROOT/"examples/sim3/fidelity-baseline-report.sample.json").read_text())
        schema=json.loads((ROOT/"schemas/software/model_fidelity_report.v1.schema.json").read_text())
        self.assertFalse(report["independent_holdout_verified"])
        self.assertEqual(report["classification"],"SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION")
        self.assertEqual(report["training_p90_abs_error_band"],2)
        self.assertEqual(report["train"]["count"],12)
        self.assertEqual(report["holdout"]["count"],16)
        self.assertEqual(report["train"]["mae"],0.75)
        self.assertEqual(report["holdout"]["mae"],1.5)
        self.assertEqual(report["holdout"]["training_p90_band_coverage"],0.75)
        self.assertEqual(len(report["rows"]),28)
        self.assertEqual(set(report["per_metric"]),{"alive","energy","learning","food"})
        self.assertEqual(report["per_metric"]["energy"]["training_band"],1)
        self.assertEqual(report["per_metric"]["energy"]["holdout"]["training_p90_band_coverage"],0.0)
        if Draft202012Validator is not None:
            Draft202012Validator.check_schema(schema)
            validator=Draft202012Validator(schema)
            validator.validate(report)
            invalid=dict(report);invalid["independent_holdout_verified"]=True
            self.assertFalse(validator.is_valid(invalid))
            invalid=dict(report);invalid["classification"]="BIOLOGICAL_PROVEN"
            self.assertFalse(validator.is_valid(invalid))
            invalid=dict(report);invalid["unverified_actuator"]="start"
            self.assertFalse(validator.is_valid(invalid))
    def test_all_fixture_references_are_synthetic(self):
        raw=(ROOT/"examples/sim3/synthetic-reference.cevobs").read_text()
        self.assertIn("origin SYNTHETIC_FIXTURE",raw)
        self.assertIn("units DIMENSIONLESS_PROXY",raw)
        self.assertNotIn("REAL_BIOLOGICAL",raw)

if __name__=="__main__":unittest.main()
