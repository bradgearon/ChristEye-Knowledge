"""V3.2 spinal Root and retention conformance: software/document checks only."""
import json
from pathlib import Path
import re
import unittest

from jsonschema import Draft202012Validator, FormatChecker, ValidationError

ROOT = Path(__file__).resolve().parents[1]

CASES = {
 "study_readiness_unapproved.example.json": "schemas/animon/study_readiness_packet.schema.json",
 "spinal_root_edge_unobserved.example.json": "schemas/animon/root_evidence_edge.schema.json",
 "axon_projection_unobserved.example.json": "schemas/biology/axon_projection.schema.json",
 "functional_root_unobserved.example.json": "schemas/biology/functional_root_assessment.schema.json"
}

def fixture(name):
 return json.loads((ROOT/"fixtures/v3.2"/name).read_text(encoding="utf-8"))

def validator(path):
 schema=json.loads((ROOT/path).read_text(encoding="utf-8"))
 Draft202012Validator.check_schema(schema)
 return Draft202012Validator(schema,format_checker=FormatChecker())

class RootRetentionV32Tests(unittest.TestCase):
 def test_synthetic_root_and_study_examples_are_schema_valid(self):
  for filename,schema in CASES.items():
   with self.subTest(filename=filename):
    validator(schema).validate(fixture(filename))

 def test_no_observed_axon_or_memory_fabricated(self):
  edge=fixture("spinal_root_edge_unobserved.example.json")
  self.assertEqual(edge["evidence_status"],"DESIGN")
  self.assertFalse(edge["claim_promoted"])
  self.assertIsNone(edge["observation_ref"])
  self.assertFalse(edge["physical_actuation_authorized"])
  axon=fixture("axon_projection_unobserved.example.json")
  self.assertEqual(axon["validity"],"UNRESOLVED")
  self.assertEqual(axon["axon_track_ids"],[])
  self.assertIsNone(axon["latency_ms"])
  root=fixture("functional_root_unobserved.example.json")
  self.assertIsNone(root["immediate_score"])
  self.assertIsNone(root["delayed_score"])
  self.assertIsNone(root["sealed_evaluation_ref"])
  self.assertEqual(root["frozen_reader_sha256"],"0"*64)
  self.assertEqual(root["interpretation"],"UNRESOLVED")

 def test_study_does_not_authorize_physical_execution(self):
  schema=validator(CASES["study_readiness_unapproved.example.json"])
  draft=fixture("study_readiness_unapproved.example.json")
  self.assertEqual(draft["status"],"DRAFT")
  self.assertIsNone(draft["preparation_ref"])
  self.assertIsNone(draft["approved_sop_ref"])
  self.assertIsNone(draft["principal_investigator_ref"])
  self.assertFalse(draft["physical_execution_authorized"])
  for patch in [
   {"physical_execution_authorized":True},
   {"status":"APPROVED_BY_LAB"},
   {"experimental_unit":"ELECTRODES_ARE_INDEPENDENT_PREPARATIONS"}
  ]:
   with self.subTest(patch=patch),self.assertRaises(ValidationError):
    schema.validate({**draft,**patch})
  schema=validator(CASES["spinal_root_edge_unobserved.example.json"])
  edge=fixture("spinal_root_edge_unobserved.example.json")
  for patch in [{"claim_promoted":True},{"physical_actuation_authorized":True}]:
   with self.subTest(patch=patch),self.assertRaises(ValidationError):
    schema.validate({**edge,**patch})

 def test_four_scientific_dossiers_have_full_distinct_pairs_and_live_links(self):
  entries={
   "CE-A09_Animon_Core":["soil","source","spinal root"],
   "CE-A17_Animon_Care":["care","chemibus","sustibus"],
   "CE-A16_Animon_Spinal_Root":["axon","evidence","spinal"],
   "CE-A02_Animon_Long_Term_Retention":["retention","root","reader"]
  }
  for name,words in entries.items():
   for folder,suffix in [("design","_Design.md"),("companion","_Scientific_Companion.md")]:
    path=ROOT/"specifications/v3.2"/folder/(name+suffix)
    with self.subTest(document=str(path)):
     self.assertTrue(path.is_file())
     raw=path.read_text(encoding="utf-8")
     self.assertGreater(len(raw),6500)
     self.assertIn("v3.2",raw.lower())
     self.assertIn("Animon",raw)
     for word in words:self.assertIn(word,raw.lower())
     # In-document anchors are excluded; external references remain external.
     for href in re.findall(r"(?<!!)\[[^\]]+\]\(([^)]+)\)",raw):
      if href.startswith(("http:","https:","mailto:","#")):continue
      target=href.split("#",1)[0]
      if not target:continue
      self.assertTrue((path.parent/target).exists(),
                      f"{path.relative_to(ROOT)} has broken link {href}")

 def test_current_index_points_to_all_four_paired_modules(self):
  index=(ROOT/"specifications/v3.2/README.md").read_text(encoding="utf-8")
  for identifier in ["CE-A09","CE-A17","CE-A16","CE-A02"]:
   self.assertIn(identifier,index)
  self.assertIn("study_readiness_packet.schema.json",index)
  self.assertIn("root_evidence_edge.schema.json",index)

if __name__=="__main__":
 unittest.main()
