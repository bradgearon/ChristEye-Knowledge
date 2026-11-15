"""Proposed colony-control schema conformance tests (external validator optional).

Normal repository tests stay stdlib-only; the dedicated controls workflow installs
JSON Schema Draft 2020-12 validation dependencies to execute all cases fully.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

try:
    from jsonschema import Draft202012Validator, FormatChecker
except ImportError:  # CI for this feature explicitly installs jsonschema.
    Draft202012Validator = None
    FormatChecker = None

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "control_profile": ("schemas/software/control_profile.v1.schema.json",
                        "examples/colony_control_profile.sample.json"),
    "crystal_session": ("schemas/software/crystal_session.v1.schema.json",
                        "examples/crystal_observation_session.sample.json"),
    "teacher_vision_session": ("schemas/software/teacher_vision_session.v1.schema.json",
                               "examples/teacher_vision_session.sample.json"),
    "board_capability": ("schemas/software/board_capability.v1.schema.json",
                         "examples/colony_board_capability.sample.json"),
    "guidance_proposal": ("schemas/software/guidance_proposal.v1.schema.json",
                          "examples/colony_guidance.sample.json"),
}
AXES = {
    "food_support", "knowledge", "growth", "understanding", "energy",
    "retention", "expressiveness", "creativity", "crystal_focus",
    "teacher_balance", "environment_adaptation", "evidence_rigor",
}


class TestControlContractShape(unittest.TestCase):
    def test_profiles_have_exactly_twelve_defined_axes(self) -> None:
        schema = json.loads((ROOT / CASES["control_profile"][0]).read_text())
        sample = json.loads((ROOT / CASES["control_profile"][1]).read_text())
        axes_schema = schema["properties"]["axes"]
        self.assertEqual(set(axes_schema["required"]), AXES)
        self.assertEqual(set(axes_schema["properties"]), AXES)
        self.assertEqual(set(sample["axes"]), AXES)
        self.assertFalse(axes_schema["additionalProperties"])
        for key, value in sample["axes"].items():
            self.assertTrue(0 <= value <= 1000, key)

    def test_examples_are_not_actuation_commands(self) -> None:
        crystal = json.loads((ROOT / CASES["crystal_session"][1]).read_text())
        teacher = json.loads((ROOT / CASES["teacher_vision_session"][1]).read_text())
        self.assertEqual(crystal["physical_action_authority"], "NONE")
        self.assertEqual(teacher["biological_action_authority"], "NONE")
        self.assertEqual(crystal["archive_class"], "NOT_ARCHIVED")

    def test_all_schemas_parse_and_declare_draft(self) -> None:
        for name, (spec, _) in CASES.items():
            with self.subTest(name=name):
                s = json.loads((ROOT / spec).read_text())
                self.assertEqual(s["$schema"], "https://json-schema.org/draft/2020-12/schema")
                self.assertFalse(s["additionalProperties"])


@unittest.skipIf(Draft202012Validator is None, "jsonschema required for full validation")
class TestControlDraft202012(unittest.TestCase):
    def schema_and_example(self, name: str):
        schema_file, example_file = CASES[name]
        schema = json.loads((ROOT / schema_file).read_text())
        instance = json.loads((ROOT / example_file).read_text())
        Draft202012Validator.check_schema(schema)
        return Draft202012Validator(schema, format_checker=FormatChecker()), instance

    def test_all_valid_examples(self) -> None:
        for name in CASES:
            with self.subTest(name=name):
                validator, instance = self.schema_and_example(name)
                validator.validate(instance)

    def test_missing_extra_and_out_of_range_axes_are_rejected(self) -> None:
        validator, profile = self.schema_and_example("control_profile")
        bad = copy.deepcopy(profile)
        del bad["axes"]["growth"]
        self.assertFalse(validator.is_valid(bad))
        bad = copy.deepcopy(profile)
        bad["axes"]["extra_priority"] = 500
        self.assertFalse(validator.is_valid(bad))
        bad = copy.deepcopy(profile)
        bad["axes"]["crystal_focus"] = 1001
        self.assertFalse(validator.is_valid(bad))

    def test_physical_material_cannot_be_installable_digital_pack(self) -> None:
        validator, session = self.schema_and_example("crystal_session")
        bad = copy.deepcopy(session)
        bad["archive_class"] = "DIGITAL_INSTALLABLE_PACK_VERIFIED"
        self.assertFalse(validator.is_valid(bad))
        bad["state"] = "RESEARCH_DOSSIER_ARCHIVED"
        self.assertFalse(validator.is_valid(bad))

    def test_teacher_requires_consent_when_active(self) -> None:
        validator, instance = self.schema_and_example("teacher_vision_session")
        bad = copy.deepcopy(instance)
        bad["consent_ref"] = None
        self.assertFalse(validator.is_valid(bad))
        bad = copy.deepcopy(instance)
        bad["state"] = "REVOKED"
        self.assertFalse(validator.is_valid(bad))

    def test_readout_is_not_unauthorized_actuation(self) -> None:
        validator, board = self.schema_and_example("board_capability")
        bad = copy.deepcopy(board)
        bad["permissions"].append("DIRECT_STIMULATION")
        self.assertFalse(validator.is_valid(bad))


if __name__ == "__main__":
    unittest.main()
