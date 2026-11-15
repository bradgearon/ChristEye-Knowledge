"""CE-SW-05 digital-only AVM configuration and routing admission reference.

This module admits or rejects proposed AUDIO_PCM/MIDI_UMP/VIDEO_FRAME routes
based on manifests. It does not stream PCM, negotiate hardware, run DSP,
connect to neural equipment or implement physical output controls.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Mapping, Any

VERIFIED = frozenset({"VERIFIED", "ACTIVE"})
ALLOWED_KINDS = frozenset({"AUDIO_PCM", "MIDI_UMP", "VIDEO_FRAME"})
CAPTURE_KINDS = frozenset({"AUDIO_PCM", "VIDEO_FRAME"})
STREAM_SOURCE_ROLES = frozenset({"PRODUCER", "BIDIRECTIONAL"})
STREAM_SINK_ROLES = frozenset({"CONSUMER", "BIDIRECTIONAL"})


@dataclass(frozen=True)
class Admission:
    accepted: bool
    reason: str


def _deny(reason: str) -> Admission:
    return Admission(False, reason)


def _by_unique_id(items: list[dict[str, Any]], key: str) -> dict[str, dict[str, Any]] | None:
    result: dict[str, dict[str, Any]] = {}
    for item in items:
        name = item.get(key)
        if not isinstance(name, str) or not name or name in result:
            return None
        result[name] = item
    return result


def _required_feature(port: Mapping[str, Any]) -> str | None:
    kind = port.get("media_kind")
    connector = port.get("connector")
    if kind == "AUDIO_PCM":
        if connector == "SOFTWARE_VIRTUAL":
            return "AUDIO_LOOPBACK"
        if port.get("direction") == "INPUT":
            return "MIC_CAPTURE" if connector in {"XLR", "COMBO_XLR_TRS"} else "LINE_CAPTURE"
        return "HEADPHONE_PLAYBACK" if connector == "HEADPHONE_TRS" else "LINE_PLAYBACK"
    if kind == "MIDI_UMP":
        return "MIDI_UMP"
    if kind == "VIDEO_FRAME":
        return "CAMERA_CAPTURE"
    return None


def _port_is_verified(manifest: Mapping[str, Any], port: Mapping[str, Any]) -> bool:
    if port.get("state") not in VERIFIED:
        return False
    if port.get("media_kind") == "NEURAL_OBSERVATION":
        return False
    feature = _required_feature(port)
    advertised = _by_unique_id(manifest.get("feature_inventory", []), "feature_id")
    if feature is None or advertised is None or advertised.get(feature, {}).get("state") not in VERIFIED:
        return False
    slot = port.get("opamp_card_slot")
    if slot is None:
        return True
    cards = _by_unique_id(manifest.get("opamp_cards", []), "slot_id")
    if cards is None or slot not in cards:
        return False
    card = cards[slot]
    return (
        card.get("state") in VERIFIED
        and card.get("calibration_digest") is not None
        and card.get("power_off_replaceable") is True
        and card.get("hot_swap_allowed") is False
        and manifest.get("hardware_revision") in card.get("compatible_carrier_revisions", [])
    )


def available_port_ids(manifest: Mapping[str, Any]) -> frozenset[str]:
    """Returns verified digital-routing ports, not physical output authorization."""
    if manifest.get("verification_state") != "AUTHORIZED":
        return frozenset()
    ports = _by_unique_id(manifest.get("ports", []), "port_id")
    if ports is None:
        return frozenset()
    return frozenset(k for k, v in ports.items() if _port_is_verified(manifest, v))


def admit_media_route(
    manifests: Mapping[str, Mapping[str, Any]],
    proposal: Mapping[str, Any],
    *,
    allowed_principals: frozenset[str],
    approved_consent_refs: frozenset[str],
    current_revision: int = 0,
) -> Admission:
    """Pure, conservative dry-run admission; never modifies a live patchbay."""
    if proposal.get("schema_version") != "ce.media.route/1":
        return _deny("BAD_SCHEMA")
    if proposal.get("biological_action_authority") != "NONE":
        return _deny("BIOLOGICAL_AUTHORITY_FORBIDDEN")
    if proposal.get("state") != "PROPOSED":
        return _deny("NOT_A_PROPOSAL")
    if proposal.get("expected_revision") != current_revision:
        return _deny("REVISION_CONFLICT")
    if proposal.get("requesting_principal") not in allowed_principals:
        return _deny("CALLER_UNAUTHORIZED")

    kind = proposal.get("media_kind")
    if kind not in ALLOWED_KINDS:
        return _deny("MEDIA_KIND_NOT_SUPPORTED")

    selected: list[tuple[Mapping[str, Any], Mapping[str, Any]]] = []
    for endpoint in ("source", "destination"):
        selector = proposal.get(endpoint)
        if not isinstance(selector, Mapping):
            return _deny("ENDPOINT_MISSING")
        manifest = manifests.get(selector.get("module_id"))
        if manifest is None:
            return _deny("MODULE_NOT_FOUND")
        if manifest.get("verification_state") != "AUTHORIZED":
            return _deny("MODULE_NOT_AUTHORIZED")
        ports = _by_unique_id(manifest.get("ports", []), "port_id")
        if ports is None:
            return _deny("PORT_INVENTORY_AMBIGUOUS")
        port = ports.get(selector.get("port_id"))
        if port is None or not _port_is_verified(manifest, port):
            return _deny("PORT_NOT_VERIFIED")
        if port.get("media_kind") != kind:
            return _deny("PORT_MEDIA_KIND_MISMATCH")
        if port.get("access_scope") != proposal.get("access_scope"):
            return _deny("ACCESS_SCOPE_MISMATCH")
        selected.append((manifest, port))

    source, target = selected[0][1], selected[1][1]
    if source.get("flow_role") not in STREAM_SOURCE_ROLES or target.get("flow_role") not in STREAM_SINK_ROLES:
        return _deny("INCOMPATIBLE_FLOW_ROLES")
    if kind in CAPTURE_KINDS:
        consent = proposal.get("teacher_consent_ref")
        if not consent or consent not in approved_consent_refs:
            return _deny("CONSENT_REQUIRED")

    if proposal.get("clock_policy") != "EXACT_SAME_CLOCK":
        # No resampler, timebase converter or asynchronous relay is implemented.
        return _deny("CLOCK_POLICY_NOT_IMPLEMENTED")
    if source.get("clock_domain") != target.get("clock_domain"):
        return _deny("CLOCK_DOMAIN_MISMATCH")

    if kind == "AUDIO_PCM":
        rate = proposal.get("sample_rate_hz")
        if not isinstance(rate, int) or rate <= 0:
            return _deny("BAD_SAMPLE_RATE")
        if rate not in source.get("sample_rates_hz", []) or rate not in target.get("sample_rates_hz", []):
            return _deny("SAMPLE_RATE_UNSUPPORTED")
        formats = set(source.get("sample_formats", [])) & set(target.get("sample_formats", []))
        if not formats:
            return _deny("FORMAT_MISMATCH")
        channels = proposal.get("channel_map", [])
        if not isinstance(channels, list) or not channels or len(channels) > min(source.get("channels", 0), target.get("channels", 0)):
            return _deny("INVALID_CHANNEL_MAP")
        if any(not isinstance(c, int) or c < 0 or c >= source.get("channels", 0) for c in channels):
            return _deny("INVALID_CHANNEL_MAP")
        if len(set(channels)) != len(channels):
            return _deny("DUPLICATE_SOURCE_CHANNEL")
    elif kind == "MIDI_UMP":
        if not set(source.get("sample_formats", [])) & set(target.get("sample_formats", [])):
            return _deny("UMP_FORMAT_INCOMPATIBLE")
        if min(source.get("midi_ump_groups", 0), target.get("midi_ump_groups", 0)) < 1:
            return _deny("NO_SHARED_UMP_GROUPS")
    elif kind == "VIDEO_FRAME":
        if not set(source.get("sample_formats", [])) & set(target.get("sample_formats", [])):
            return _deny("VIDEO_FORMAT_INCOMPATIBLE")

    return Admission(True, "DRY_RUN_ADMITTED")
