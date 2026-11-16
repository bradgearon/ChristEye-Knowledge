"""Animon v3.2 contract tests. Data validation is NOT a biological result."""
from __future__ import annotations
import copy
import json
from pathlib import Path
import re
import unittest

from jsonschema import Draft202012Validator, FormatChecker, ValidationError

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "animon_digital_reference.example.json": ("schemas/animon/animon_spec.schema.json", None),
    "chemibus_unmeasured.example.json": ("schemas/biology/chemibus_observation.schema.json", "processes/BIO-04-sustibus-chemibus.md"),
    "physical_crystal_unvalidated.example.json": ("schemas/biology/physical_crystal_candidate.schema.json", "processes/BIO-08-crystals.md"),
    "crest_birth_unobserved.example.json": ("schemas/biology/cell_birth_event.schema.json", "processes/BIO-01-crest-lineage.md"),
    "soil_region_unobserved.example.json": ("schemas/biology/soil_region.schema.json", None),
    "sustibus_unqualified.example.json": ("schemas/biology/sustibus_care_status.schema.json", None),
}

def fixture(name):
    return json.loads((ROOT / "fixtures/v3.2" / name).read_text(encoding="utf-8"))

def validator(source):
    schema = json.loads((ROOT / source).read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(schema)
    return Draft202012Validator(schema, format_checker=FormatChecker())

class AnimonV32Contracts(unittest.TestCase):
    def test_each_example_validates_against_authoritative_schema(self):
        for name, (source, _) in CASES.items():
            with self.subTest(name=name):
                validator(source).validate(fixture(name))

    def test_process_examples_match_canonical_fixture_byte_semantics(self):
        for name, (_, doc) in CASES.items():
            if not doc:
                continue
            with self.subTest(doc=doc):
                blocks = re.findall(r"~~~json\n([\s\S]*?)\n~~~",
                                    (ROOT / doc).read_text(encoding="utf-8"))
                self.assertTrue(blocks, "Missing JSON example")
                self.assertEqual(json.loads(blocks[0]), fixture(name))

    def test_synthetic_examples_cannot_be_misread_as_observed(self):
        for name in list(CASES)[1:]:
            record = fixture(name)
            self.assertEqual(record["evidence_status"], "DESIGN")
            self.assertIsNone(record["evidence_ref"])
            self.assertEqual(record["preparation_id"], "PREP-NOT-CREATED-SYNTHETIC")
        self.assertIsNone(fixture("chemibus_unmeasured.example.json")["observed_value"])
        self.assertFalse(fixture("chemibus_unmeasured.example.json")["physical_actuation_allowed"])
        self.assertFalse(fixture("physical_crystal_unvalidated.example.json")["installable"])
        self.assertEqual(fixture("crest_birth_unobserved.example.json")["daughter_observation_ids"], [])

    def test_malformed_biological_records_are_rejected(self):
        chemi = validator("schemas/biology/chemibus_observation.schema.json")
        base = fixture("chemibus_unmeasured.example.json")
        for patch in [{"physical_actuation_allowed": True},
                      {"schema_version": "chemibus.v1"},
                      {"unexpected_field": "unqualified"}]:
            with self.subTest(patch=patch), self.assertRaises(ValidationError):
                chemi.validate({**base, **patch})
        with self.assertRaises(ValidationError):
            invalid = copy.deepcopy(base)
            invalid["analyte"] = invalid.pop("analyte_identity")
            chemi.validate(invalid)
        crystal = validator("schemas/biology/physical_crystal_candidate.schema.json")
        with self.assertRaises(ValidationError):
            crystal.validate({**fixture("physical_crystal_unvalidated.example.json"), "installable": True})

    def test_digital_model_cannot_claim_an_individual_living_animon(self):
        schema = validator("schemas/animon/animon_spec.schema.json")
        base = fixture("animon_digital_reference.example.json")
        self.assertEqual(base["system_class"], "ANIMON")
        self.assertEqual(base["embodiment"], "DIGITAL_REFERENCE")
        self.assertIsNone(base["candidate_instance_id"])
        self.assertFalse(base["physical_actuation_authorized"])
        for patch in [{"candidate_instance_id": "CLAIMED-LIVE"},
                      {"source_lot_refs": ["FICTIONAL-LINE"]},
                      {"physical_actuation_authorized": True}]:
            with self.subTest(patch=patch), self.assertRaises(ValidationError):
                schema.validate({**base, **patch})

    def test_first_two_pilot_dossiers_are_genuinely_module_specific(self):
        pairs = {
            "CE-A09_Animon_Core": [
                "Soil", "spinal Root", "preparation_id", "source", "Animon",
            ],
            "CE-A17_Animon_Care": [
                "ChemiBus", "SustiBus", "care", "analyte", "Animon",
            ],
        }
        for prefix, words in pairs.items():
            design = (ROOT / "specifications/v3.2/design" /
                      (prefix + "_Design.md")).read_text(encoding="utf-8")
            companion = (ROOT / "specifications/v3.2/companion" /
                         (prefix + "_Scientific_Companion.md")).read_text(encoding="utf-8")
            self.assertGreater(len(design), 5500, prefix)
            self.assertGreater(len(companion), 5500, prefix)
            for word in words:
                self.assertIn(word.lower(), (design + companion).lower())
            self.assertIn("v3.2", design.lower())
            self.assertIn("v3.2", companion.lower())
            self.assertIn("not", companion.lower())

    def test_ontology_source_and_lab_standard_remain_linked(self):
        ontology = (ROOT / "ontology/ANIMON_V1.md").read_text(encoding="utf-8")
        for phrase in ["ChristEye Anaiero", "Animon specification", "Soil",
                       "Spinal Root", "digital_model_id"]:
            self.assertIn(phrase, ontology)
        self.assertTrue((ROOT / "reviews/V3_2_LAB_DOSSIER_STANDARD.md").is_file())

if __name__ == "__main__":
    unittest.main()
