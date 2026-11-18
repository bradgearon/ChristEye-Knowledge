"""Animon v3.3 laboratory work orders: documentation and read-only QC tests.

These tests do not validate laboratory operations, biological recipes or safe ranges.
"""
from __future__ import annotations
import copy
import importlib.util
import json
from pathlib import Path
import re
import unittest

from jsonschema import Draft202012Validator, FormatChecker

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "laboratory"
FILES = [
 "ANIMON_LAB_OPERATIONS_v3.3.md",
 "WO-00-source-and-genomic-qc.md",
 "WO-01-soil-development-and-maturation.md",
 "WO-02-daily-care-and-chemistry.md",
 "WO-03-axon-and-spinal-root-evidence.md",
 "WO-04-learning-and-delayed-retention.md"
]
module_path = ROOT / "qa" / "daily_qc.py"
spec = importlib.util.spec_from_file_location("animon_lab_daily_qc",module_path)
lab = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(lab)
def sample():
 return json.loads((ROOT/"fixtures/v3.3/synthetic_daily_instrument_qc.example.json").read_text(encoding="utf-8"))

class LabWorkOrdersV33(unittest.TestCase):
 def test_all_six_operational_documents_and_steps_exist(self):
  for name in FILES:
   path=WORK/name
   with self.subTest(doc=name):
    self.assertTrue(path.is_file())
    txt=path.read_text(encoding="utf-8")
    self.assertGreater(len(txt),5000)
    self.assertIn("SOP",txt)
    self.assertTrue("HOLD" in txt or "UNRESOLVED" in txt)
    self.assertTrue("source" in txt.lower())
    self.assertTrue("previous" in txt.lower() or "prior" in txt.lower())

 def test_work_order_links_resolve(self):
  pattern=re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
  for name in FILES:
   path=WORK/name
   txt=path.read_text(encoding="utf-8")
   for href in pattern.findall(txt):
    if href.startswith(("http://","https://","mailto:","#")):continue
    target=href.split("#",1)[0]
    if not target:continue
    self.assertTrue((path.parent/target).is_file(),
                    f"{name} has broken link to {href}")

 def test_daily_fixture_is_valid_and_nonbiological(self):
  fixture=sample()
  schema=json.loads((ROOT/"schemas/animon/daily_qc_observation.schema.json").read_text(encoding="utf-8"))
  Draft202012Validator.check_schema(schema)
  Draft202012Validator(schema,format_checker=FormatChecker()).validate(fixture)
  self.assertEqual(fixture["current"]["culture_stage"],"NONBIOLOGICAL-QA")
  self.assertEqual(fixture["current"]["medium_lot"],"NOT_A_CULTURE")
  self.assertIn("SYNTHETIC",fixture["approved_limits"]["approved_sop_ref"])

 def test_comparable_day_delta_and_screening(self):
  output=lab.check(sample())
  self.assertEqual(output["action_class"],"NORMAL_RECORD")
  self.assertAlmostEqual(output["delta_absolute"],0.03)
  self.assertAlmostEqual(output["delta_percent"],(0.03/1.01)*100,places=5)
  self.assertFalse(output["physical_actuation"])

 def test_outside_approved_synthetic_critical_bounds_blocks_new_study(self):
  record=sample()
  record["current"]["value"]=1.27
  output=lab.check(record)
  self.assertEqual(output["action_class"],"HOLD_NEW_EXPERIMENT")
  self.assertFalse(output["physical_actuation"])

 def test_warning_requiring_scientist_review(self):
  record=sample()
  record["current"]["value"]=1.13
  output=lab.check(record)
  self.assertEqual(output["action_class"],"REVIEW_TREND")
  self.assertFalse(output["physical_actuation"])

 def test_missing_care_receipt_is_not_inferred_as_completed(self):
  record=sample()
  record["current"]["care_due"]=True
  record["current"]["verified_care_receipt"]=None
  output=lab.check(record)
  self.assertEqual(output["action_class"],"HOLD_NEW_EXPERIMENT")

 def test_invalid_calibration_has_no_biological_result(self):
  record=sample()
  record["current"]["calibration_valid"]=False
  output=lab.check(record)
  self.assertEqual(output["action_class"],"INVALID_MEASUREMENT")
  self.assertIsNone(output["delta_percent"])

 def test_changed_medium_lot_is_not_directly_comparable(self):
  record=sample()
  record["current"]["medium_lot"]="SYNTHETIC-CHANGED-LOT"
  output=lab.check(record)
  self.assertEqual(output["action_class"],"NONCOMPARABLE")
  self.assertIsNone(output["delta_absolute"])

 def test_unsourced_limits_are_not_silent_normality(self):
  record=sample()
  record["approved_limits"]["approved_sop_ref"]=None
  output=lab.check(record)
  self.assertEqual(output["action_class"],"UNRESOLVED")

 def test_missing_previous_data_is_unknown(self):
  record=sample()
  record["previous"]=None
  output=lab.check(record)
  self.assertEqual(output["action_class"],"UNRESOLVED")
  self.assertIsNone(output["delta_percent"])

 def test_no_actuation_calls_in_qa_code(self):
  source=module_path.read_text(encoding="utf-8")
  for token in ["serial.Serial(","subprocess.run(","requests.post(","urllib.request.urlopen("]:
   self.assertNotIn(token,source)
  self.assertIn('"physical_actuation": False',source)

if __name__=="__main__":
 unittest.main()
