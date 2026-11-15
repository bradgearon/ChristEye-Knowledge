"""CE-SW-05: AVM media contracts and conservative digital route admission tests."""
from __future__ import annotations
from pathlib import Path
import copy
import json
import unittest

from anaiero.media import admit_media_route, available_port_ids

try:
    from jsonschema import Draft202012Validator, FormatChecker
except ImportError:
    Draft202012Validator = None
    FormatChecker = None

ROOT = Path(__file__).resolve().parents[1]
DIGEST = "a" * 64

EXAMPLES = [
    "avm_v1_voicebridge.sample.json",
    "avm_s8_studiomesh.sample.json",
    "avm_r16_researchmedia.sample.json",
    "avm_software_voice_adapter.sample.json",
]


def load_sample(name: str):
    return json.loads((ROOT / "examples" / name).read_text(encoding="utf-8"))


def working_pair():
    """Fabricate test-only VERIFIED manifests; NEVER a real board enrolment."""
    voice = load_sample(EXAMPLES[0])
    virtual = load_sample(EXAMPLES[3])
    for m in (voice, virtual):
        m["verification_state"] = "AUTHORIZED"
        for p in m["ports"]:
            p["state"] = "VERIFIED"
        for card in m["opamp_cards"]:
            card["state"] = "VERIFIED"
            card["calibration_digest"] = DIGEST
        for feature in m["feature_inventory"]:
            feature["state"] = "VERIFIED"
    route = load_sample("avm_teacher_audio_route.sample.json")
    route["teacher_consent_ref"] = "consent-ok"
    manifests = {m["module_id"]: m for m in (voice, virtual)}
    return manifests, route


def admit(manifests, route):
    return admit_media_route(
        manifests, route,
        allowed_principals=frozenset({"example-reviewer"}),
        approved_consent_refs=frozenset({"consent-ok"}),
        current_revision=0,
    )


class MediaConfigurationTest(unittest.TestCase):
    def test_examples_parse(self):
        for name in EXAMPLES + ["avm_teacher_audio_route.sample.json"]:
            with self.subTest(name=name):
                data = load_sample(name)
                self.assertEqual(data["biological_action_authority"], "NONE")

    def test_unknown_hardware_is_never_available(self):
        for name in EXAMPLES:
            with self.subTest(name=name):
                self.assertFalse(available_port_ids(load_sample(name)))

    def test_verified_card_and_permissions_admit_digital_route(self):
        manifests, route = working_pair()
        self.assertIn("mic-pair-0", available_port_ids(manifests["example-avm-v1"]))
        self.assertTrue(admit(manifests, route).accepted)

    def test_missing_consent_fails(self):
        manifests, route = working_pair()
        route["teacher_consent_ref"] = None
        self.assertEqual(admit(manifests, route).reason, "CONSENT_REQUIRED")

    def test_catalog_only_neural_output_cannot_be_routed(self):
        manifests, route = working_pair()
        route["media_kind"] = "NEURAL_OBSERVATION"
        self.assertEqual(admit(manifests, route).reason, "MEDIA_KIND_NOT_SUPPORTED")

    def test_hardware_card_removed_disables_port(self):
        manifests, route = working_pair()
        manifests["example-avm-v1"]["opamp_cards"][0]["state"] = "REMOVED"
        self.assertEqual(admit(manifests, route).reason, "PORT_NOT_VERIFIED")

    def test_catalog_feature_alone_cannot_enable_port(self):
        manifests, route = working_pair()
        flags = manifests["example-avm-v1"]["feature_inventory"]
        next(f for f in flags if f["feature_id"] == "MIC_CAPTURE")["state"] = "CATALOG_ONLY"
        self.assertEqual(admit(manifests, route).reason, "PORT_NOT_VERIFIED")

    def test_hardware_card_requires_calibration(self):
        manifests, route = working_pair()
        manifests["example-avm-v1"]["opamp_cards"][0]["calibration_digest"] = None
        self.assertEqual(admit(manifests, route).reason, "PORT_NOT_VERIFIED")

    def test_bad_principal_and_device_auth_fail(self):
        manifests, route = working_pair()
        route["requesting_principal"] = "untrusted"
        self.assertEqual(admit(manifests, route).reason, "CALLER_UNAUTHORIZED")
        manifests, route = working_pair()
        manifests["example-avm-v1"]["verification_state"] = "REVOKED"
        self.assertEqual(admit(manifests, route).reason, "MODULE_NOT_AUTHORIZED")

    def test_bad_clock_rate_and_channel_layout_fail(self):
        manifests, route = working_pair()
        route["sample_rate_hz"] = 192000
        self.assertEqual(admit(manifests, route).reason, "SAMPLE_RATE_UNSUPPORTED")
        manifests, route = working_pair()
        route["channel_map"] = [0, 0]
        self.assertEqual(admit(manifests, route).reason, "DUPLICATE_SOURCE_CHANNEL")
        manifests, route = working_pair()
        manifests["example-avm-software"]["ports"][0]["clock_domain"] = "another"
        self.assertEqual(admit(manifests, route).reason, "CLOCK_DOMAIN_MISMATCH")

    def test_wrong_logical_role_fails_even_if_connector_matches(self):
        manifests, route = working_pair()
        manifests["example-avm-software"]["ports"][0]["flow_role"] = "PRODUCER"
        self.assertEqual(admit(manifests, route).reason, "INCOMPATIBLE_FLOW_ROLES")

    def test_version_conflict_fails(self):
        manifests, route = working_pair()
        route["expected_revision"] = 99
        self.assertEqual(admit(manifests, route).reason, "REVISION_CONFLICT")


@unittest.skipIf(Draft202012Validator is None, "install jsonschema for full contract tests")
class MediaSchemaTest(unittest.TestCase):
    def test_fixture_contracts(self):
        for spec, fixtures in [
            ("media_module.v1.schema.json", EXAMPLES),
            ("media_route.v1.schema.json", ["avm_teacher_audio_route.sample.json"]),
        ]:
            schema = json.loads((ROOT / "schemas/software" / spec).read_text())
            Draft202012Validator.check_schema(schema)
            validator = Draft202012Validator(schema, format_checker=FormatChecker())
            for example in fixtures:
                with self.subTest(schema=spec, example=example):
                    validator.validate(load_sample(example))

    def test_no_hot_swap_or_biological_actuation(self):
        schema = json.loads((ROOT / "schemas/software/media_module.v1.schema.json").read_text())
        validator = Draft202012Validator(schema)
        bad = load_sample(EXAMPLES[0])
        bad["opamp_cards"][0]["hot_swap_allowed"] = True
        self.assertFalse(validator.is_valid(bad))
        bad = load_sample(EXAMPLES[0])
        bad["biological_action_authority"] = "STIMULATE"
        self.assertFalse(validator.is_valid(bad))
        bad = load_sample(EXAMPLES[2])
        obs = next(p for p in bad["ports"] if p["media_kind"] == "NEURAL_OBSERVATION")
        obs["direction"] = "OUTPUT"
        self.assertFalse(validator.is_valid(bad))

    def test_schema_rejects_unsupported_model_and_midi_mislabel(self):
        schema = json.loads((ROOT / "schemas/software/media_module.v1.schema.json").read_text())
        validator = Draft202012Validator(schema)
        bad = load_sample(EXAMPLES[0])
        bad["model_id"] = "MADE-UP-BOARD"
        self.assertFalse(validator.is_valid(bad))
        bad = load_sample(EXAMPLES[0])
        midi = next(p for p in bad["ports"] if p["media_kind"] == "MIDI_UMP")
        del midi["midi_ump_groups"]
        self.assertFalse(validator.is_valid(bad))


if __name__ == "__main__":
    unittest.main()
