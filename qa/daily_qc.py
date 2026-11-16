#!/usr/bin/env python3
"""Read-only ANIMON/3.3 daily QC: data quality and prior-day comparison.

No device, network, stimulation, culture or chemical operations. Source-specific
biological limits MUST be approved by the laboratory; never hardcode them here.
Input: JSON document with current, previous, limits, baselines, change history.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
from statistics import median
import sys

COMPARE_KEYS = (
    "preparation_id", "source_lot_id", "culture_stage", "measurement_name",
    "unit", "sample_location", "method_ref", "instrument_ref",
    "calibration_version", "medium_lot",
)
VALIDITY = {"VALID", "INVALID", "UNMEASURED"}

def must_number(x):
    return x is not None and isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x)

def get_date(s):
    if not isinstance(s, str) or not s:
        raise ValueError("timestamp is missing")
    value = datetime.fromisoformat(s.replace("Z", "+00:00"))
    if value.tzinfo is None:
        raise ValueError("timestamp must have explicit timezone")
    return value.astimezone(timezone.utc)

def comparable(a, b):
    return all(a.get(k) == b.get(k) and a.get(k) not in (None, "")
               for k in COMPARE_KEYS)

def check(record: dict) -> dict:
    now = record.get("current", {})
    prev = record.get("previous")
    limits = record.get("approved_limits", {})
    baseline = record.get("qualified_baseline", [])
    issues = []
    outcome = {
        "preparation_id": now.get("preparation_id"),
        "measurement_name": now.get("measurement_name"),
        "action_class": "UNRESOLVED",
        "delta_absolute": None,
        "delta_percent": None,
        "robust_deviation": None,
        "issues": issues,
        "physical_actuation": False,
    }
    # No scientific inference before ensuring current observation was real and valid.
    if now.get("validity") not in VALIDITY or now.get("validity") != "VALID":
        outcome["action_class"] = "INVALID_MEASUREMENT"
        issues.append("No valid current measured value; do not report a biological state.")
        return outcome
    if not must_number(now.get("value")):
        outcome["action_class"] = "INVALID_MEASUREMENT"
        issues.append("Current value absent or nonfinite.")
        return outcome
    try:
        current_time = get_date(now.get("timestamp"))
    except (ValueError, TypeError):
        outcome["action_class"] = "INVALID_MEASUREMENT"
        issues.append("Invalid or unqualified current timestamp.")
        return outcome
    if now.get("calibration_valid") is not True or not now.get("raw_data_ref"):
        outcome["action_class"] = "INVALID_MEASUREMENT"
        issues.append("Actual calibration or raw source evidence is not valid.")
        return outcome
    if now.get("care_due") and not now.get("verified_care_receipt"):
        outcome["action_class"] = "HOLD_NEW_EXPERIMENT"
        issues.append("Care service is due but actual physical receipt is missing. Escalate under approved SOP.")
        return outcome

    # Comparability precedes difference calculations: do not compare different matrices.
    if now.get("change_event_ref") and not now.get("method_bridge_approved", False):
        outcome["action_class"] = "NONCOMPARABLE"
        issues.append("Change or intervention recorded without method bridge.")
        return outcome
    if not isinstance(prev, dict) or prev.get("validity") != "VALID" or not must_number(prev.get("value")):
        issues.append("No valid previous comparable measurement.")
    elif not comparable(now, prev):
        outcome["action_class"] = "NONCOMPARABLE"
        issues.append("Preparation, source, stage, medium lot, sample, method, instrument or unit changed.")
        return outcome
    else:
        try:
            previous_time = get_date(prev.get("timestamp"))
        except (ValueError, TypeError):
            outcome["action_class"] = "NONCOMPARABLE"
            issues.append("Previous timestamp invalid.")
            return outcome
        hours = (current_time - previous_time).total_seconds() / 3600
        if hours <= 0 or hours > 48:
            issues.append("Prior reading not from the required comparable recent interval.")
        else:
            delta = now["value"] - prev["value"]
            outcome["delta_absolute"] = round(delta, 8)
            if prev["value"] != 0:
                outcome["delta_percent"] = round(100 * delta / abs(prev["value"]), 6)

    lo = limits.get("critical_low")
    hi = limits.get("critical_high")
    if not (must_number(lo) and must_number(hi) and lo < hi and limits.get("approved_sop_ref")):
        issues.append("Approved source/stage limits absent; do not infer safe biological range.")
        return outcome
    if now["value"] < lo or now["value"] > hi:
        outcome["action_class"] = "HOLD_NEW_EXPERIMENT"
        issues.append("Critical source-qualified SOP bound exceeded; notify care lead, no automatic correction.")
        return outcome

    wlo, whi = limits.get("warning_low"), limits.get("warning_high")
    warn = (must_number(wlo) and now["value"] < wlo) or (must_number(whi) and now["value"] > whi)
    if len(baseline) >= 5 and all(must_number(x) for x in baseline):
        center = median(baseline)
        mad = median(abs(x - center) for x in baseline)
        if mad > 0:
            outcome["robust_deviation"] = round(abs(now["value"] - center) / (1.4826 * mad), 5)
    if warn:
        outcome["action_class"] = "REVIEW_TREND"
        issues.append("Within critical limits but outside laboratory-approved warning band.")
    elif outcome["delta_absolute"] is None:
        outcome["action_class"] = "UNRESOLVED"
        issues.append("No qualified comparison to the previous reading.")
    else:
        outcome["action_class"] = "NORMAL_RECORD"
    return outcome

def main():
    p = argparse.ArgumentParser(description="Read-only Animon daily QC; never controls culture hardware")
    p.add_argument("source", type=Path)
    p.add_argument("--out", type=Path)
    args = p.parse_args()
    result = check(json.loads(args.source.read_text(encoding="utf-8")))
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    if result["action_class"] in ("HOLD_NEW_EXPERIMENT", "INVALID_MEASUREMENT"):
        return 2
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
