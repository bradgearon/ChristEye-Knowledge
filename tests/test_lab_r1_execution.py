"""Laboratory R1 operational packet tests: SYNTHETIC QA ONLY, no live tissue."""
from __future__ import annotations
import csv
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path
import unittest
from lab.scripts.check_daily_records import check

ROOT=Path(__file__).resolve().parents[1]
LAB=ROOT/"lab"

def bands():
    # Test-only fictional bands (NEVER for live laboratory use).
    return {
      "status":"QUALIFIED",
      "selected_vendor_protocol_ref":"TEST-ONLY-SOP",
      "approved_local_sop_ref":"TEST-ONLY-APPROVAL",
      "owner":"UNIT-TEST",
      "approved_at_utc":"2026-11-16T00:00:00Z",
      "parameters":{
        "test_metric":{
          "unit":"arb-test-units","sample_location":"test-channel",
          "assay_id":"test-v1","lower":10,"upper":20,
          "max_abs_day_change":2,"critical":True
        }
      }
    }
def row(id,day,value,epoch="baseline",status="MEASURED",**overrides):
    result={
        "campaign_id":"SYNTHETIC", "observation_id":id,
        "observed_at_utc":f"2026-11-{day:02d}T12:00:00Z",
        "plate_id":"TEST-PLATE", "well_id":"A1", "source_lot_id":"SYNTHETIC-LOT",
        "parameter":"test_metric", "value":str(value),
        "unit":"arb-test-units", "measurement_status":status,
        "assay_id":"test-v1", "instrument_id":"TEST",
        "sample_location":"test-channel", "calibration_ref":"TEST-CAL",
        "raw_data_ref":"test-data-uri", "care_receipt_ref":"TEST-SERVICE",
        "comparability_epoch":epoch,"operator_id":"UNIT-TEST"
    }
    result.update(overrides)
    return result

class LabR1ExecutionTests(unittest.TestCase):
    def test_source_campaign_handoff_documents_present(self):
        for name in ("01_SOURCE_AND_MATERIAL_RELEASE.md",
          "02_DAILY_OPERATION_AND_QC.md",
          "03_MEA_AND_EVALUATION.md",
          "04_DEVIATIONS_AND_CHANGE_CONTROL.md"):
            raw=(LAB/name).read_text(encoding="utf8")
            self.assertGreater(len(raw),5000,name)
            self.assertIn("We Develop Software LLC",raw)
            self.assertIn("SOP",raw)
        source=(LAB/"01_SOURCE_AND_MATERIAL_RELEASE.md").read_text()
        self.assertIn("BX-0100-XX",source)
        self.assertIn("BX-06XX-XX",source)
        self.assertIn("STR",source)

    def test_24well_map_has_six_each_and_is_not_approved(self):
        with (LAB/"forms/24well_allocation_PROPOSAL.csv").open() as f:
            rows=list(csv.DictReader(f))
        self.assertEqual(len(rows),24)
        self.assertEqual(len({r["well_id"] for r in rows}),24)
        self.assertEqual(set(Counter(r["proposed_group"] for r in rows).values()),{6})
        self.assertTrue(all(r["allocation_status"]=="UNAPPROVED" for r in rows))
        self.assertTrue(all(r["source_lot_id"]=="SOURCE_LOT_NOT_SELECTED" for r in rows))

    def test_numeric_form_and_nonqualified_example_ranges(self):
        with (LAB/"forms/numeric_observation_template.csv").open() as f:
            headers=next(csv.reader(f))
        for field in ["observed_at_utc","well_id","source_lot_id","parameter","value",
                      "unit","calibration_ref","raw_data_ref","care_receipt_ref",
                      "comparability_epoch","measurement_status"]:
            self.assertIn(field,headers)
        config=json.loads((LAB/"forms/qualified_parameter_bands.example.json").read_text())
        self.assertEqual(config["status"],"UNQUALIFIED_EXAMPLE")
        for v in config["parameters"].values():
            self.assertIsNone(v["lower"])
            self.assertIsNone(v["upper"])
        decision=check([row("id-1",16,15)],config)
        self.assertEqual(decision["overall"],"REQUIRES_HUMAN_REVIEW")
        self.assertEqual(decision["records"][0]["state"],"UNQUALIFIED_RANGE")

    def test_previous_day_valid_reading_within_qualified_band(self):
        decision=check([row("id-1",16,15),row("id-2",17,16)],bands())
        self.assertEqual(decision["records"][0]["state"],"BASELINE_ONLY")
        r=decision["records"][1]
        self.assertEqual(r["state"],"WITHIN_SIGNED_BAND")
        self.assertEqual(r["previous_observation_id"],"id-1")
        self.assertEqual(r["delta"],1.0)
        self.assertAlmostEqual(r["delta_percent"],100/15,places=5)
        self.assertFalse(r["physical_action_authorized"])

    def test_outside_range_or_rapid_drift_requires_review(self):
        decision=check([row("id-1",16,15),row("id-2",17,19)],bands())
        self.assertEqual(decision["records"][1]["state"],"DEVIATION_REVIEW")
        self.assertTrue(decision["records"][1]["hold_new_experiments"])
        decision=check([row("id-1",16,15),row("id-2",17,25)],bands())
        self.assertEqual(decision["records"][1]["state"],"DEVIATION_REVIEW")
        self.assertTrue(decision["records"][1]["review_required"])

    def test_service_epoch_changes_break_comparability(self):
        decision=check([row("id-1",16,15,"before-service"),
                        row("id-2",17,15.5,"after-service")],bands())
        self.assertIsNone(decision["records"][1]["previous_observation_id"])
        self.assertIsNone(decision["records"][1]["delta"])
        self.assertEqual(decision["records"][1]["state"],"BASELINE_ONLY")
        self.assertTrue(decision["records"][1]["review_required"])

    def test_missing_or_invalid_source_does_not_become_zero(self):
        x=check([row("id-1",16,"",status="NOT_MEASURED")],bands())
        self.assertEqual(x["records"][0]["state"],"NOT_MEASURED")
        self.assertTrue(x["records"][0]["hold_new_experiments"])
        x=check([row("id-1",16,15,calibration_ref="")],bands())
        self.assertEqual(x["records"][0]["state"],"INVALID_MEASUREMENT")
        x=check([row("id-1",16,15),row("id-1",17,15)],bands())
        self.assertTrue(any(r["state"]=="INVALID_RECORD" for r in x["records"]))

    def test_every_qa_result_is_readonly(self):
        data=check([row("id-1",16,15),row("id-2",17,17)],bands())
        self.assertFalse(data["physical_actuation_authorized"])
        self.assertEqual(data["mode"],"READ_ONLY")
        self.assertTrue(all(r["physical_action_authorized"] is False for r in data["records"]))
        self.assertIn("never authorizes",data["operator_note"])

if __name__=="__main__":
    unittest.main()
