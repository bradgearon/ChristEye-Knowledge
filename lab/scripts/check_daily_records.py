#!/usr/bin/env python3
"""Anaiero LAB-R1 daily quality review (read-only).

Reads deidentified numeric observations and a lab-SIGNED qualification config;
compares each valid observation with the previous comparable observation,
flags drift/invalid data, and prints an auditable JSON report.

NEVER commands a physical device or authorizes a biological intervention.
The example band file is UNQUALIFIED and deliberately cannot produce PASS.
"""
from __future__ import annotations

import argparse
import csv
from collections import Counter, defaultdict
from datetime import datetime
import json
import math
from pathlib import Path
import sys

REQUIRED = {
    "campaign_id", "observation_id", "observed_at_utc", "plate_id", "well_id",
    "source_lot_id", "parameter", "value", "unit", "measurement_status",
    "assay_id", "instrument_id", "sample_location", "calibration_ref",
    "raw_data_ref", "care_receipt_ref", "comparability_epoch", "operator_id",
}
ALLOWED_MEASUREMENTS = {"MEASURED", "NOT_MEASURED", "INVALID", "UNKNOWN"}
COMPARABLE_KEYS = ("plate_id","well_id","source_lot_id","parameter",
                   "unit","assay_id","sample_location","comparability_epoch")


def when(value):
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        if not parsed.tzinfo:
            raise ValueError("Timezone missing")
        return parsed
    except (AttributeError, ValueError):
        raise ValueError("Observed_at must be a timezone-aware ISO 8601 string")


def finite_number(value):
    try:
        n = float(value)
    except (ValueError, TypeError):
        raise ValueError("No finite numeric value")
    if not math.isfinite(n):
        raise ValueError("Not finite")
    return n


def qualified(bands):
    return (
        bands.get("status") == "QUALIFIED"
        and bool(bands.get("selected_vendor_protocol_ref"))
        and bool(bands.get("approved_local_sop_ref"))
        and bool(bands.get("owner"))
        and bool(bands.get("approved_at_utc"))
    )


def check(rows, bands):
    """Return review records. Caller is responsible for a named human decision."""
    requirements_met = qualified(bands)
    seen_ids = set()
    observed = defaultdict(list)
    results = []
    for row in sorted(rows, key=lambda r: (r.get("observed_at_utc", ""), r.get("observation_id", ""))):
        rid = row.get("observation_id", "")
        p = row.get("parameter", "")
        specification = bands.get("parameters", {}).get(p)
        status = row.get("measurement_status", "")
        result = {
            "observation_id": rid, "plate_id": row.get("plate_id"),
            "well_id": row.get("well_id"), "parameter": p,
            "state": "UNRESOLVED", "previous_observation_id": None,
            "delta": None, "delta_percent": None,
            "review_required": True, "hold_new_experiments": True,
            "reason": [], "physical_action_authorized": False,
        }
        results.append(result)
        if not rid or rid in seen_ids:
            result["state"] = "INVALID_RECORD"
            result["reason"].append("Missing or duplicate observation ID")
            continue
        seen_ids.add(rid)
        try:
            t = when(row.get("observed_at_utc", ""))
        except ValueError as err:
            result["state"] = "INVALID_RECORD"
            result["reason"].append(str(err))
            continue
        if not specification:
            result["state"] = "UNQUALIFIED_PARAMETER"
            result["reason"].append("Parameter not defined by controlled range file")
            continue
        if not requirements_met or any(specification.get(field) is None
            for field in ("lower","upper","max_abs_day_change","unit","sample_location")):
            result["state"] = "UNQUALIFIED_RANGE"
            result["reason"].append("No signed source-specific approved reference band")
            continue
        if status not in ALLOWED_MEASUREMENTS:
            result["state"] = "INVALID_RECORD"
            result["reason"].append("Unknown measurement status")
            continue
        if status != "MEASURED":
            result["state"] = status
            result["reason"].append("No valid observation; do not substitute zero")
            continue
        if row.get("unit") != specification.get("unit") or row.get("sample_location") != specification.get("sample_location"):
            result["state"] = "INVALID_MEASUREMENT"
            result["reason"].append("Unit or sampling location does not match qualification")
            continue
        if specification.get("assay_id") and row.get("assay_id") != specification.get("assay_id"):
            result["state"] = "INVALID_MEASUREMENT"
            result["reason"].append("Assay is not the qualified assay")
            continue
        if not row.get("raw_data_ref") or not row.get("calibration_ref") or not row.get("operator_id"):
            result["state"] = "INVALID_MEASUREMENT"
            result["reason"].append("Raw data, calibration, or operator provenance missing")
            continue
        try:
            val = finite_number(row.get("value", ""))
            lower = finite_number(specification["lower"])
            upper = finite_number(specification["upper"])
            drift = finite_number(specification["max_abs_day_change"])
            assert lower <= upper and drift >= 0
        except (ValueError, AssertionError):
            result["state"] = "INVALID_RANGE_OR_VALUE"
            result["reason"].append("Bad or nonfinite value/qualified range")
            continue
        key = tuple(row.get(k) for k in COMPARABLE_KEYS)
        history = observed[key]
        if val < lower or val > upper:
            result["reason"].append("Outside signed band")
        if history:
            prev, prev_time, prev_val = history[-1]
            result["previous_observation_id"] = prev
            days = (t.date() - prev_time.date()).days
            if days == 1:
                delta = val - prev_val
                result["delta"] = round(delta, 9)
                result["delta_percent"] = None if prev_val == 0 else round(100 * delta / abs(prev_val), 5)
                if abs(delta) > drift:
                    result["reason"].append("Exceeds approved previous-day absolute-change band")
            elif days == 0:
                result["reason"].append("Same-day comparison; no prior-day delta")
            else:
                result["reason"].append("Previous valid comparable observation is not from previous day")
        else:
            result["reason"].append("No previous comparable observation: new baseline/epoch")
        history.append((rid, t, val))
        if any(x.startswith(("Outside signed","Exceeds approved")) for x in result["reason"]):
            result["state"] = "DEVIATION_REVIEW"
        elif not history[:-1]:
            result["state"] = "BASELINE_ONLY"
        elif result["delta"] is None:
            result["state"] = "COMPARABILITY_REVIEW"
        else:
            result["state"] = "WITHIN_SIGNED_BAND"
            result["review_required"] = False
            result["hold_new_experiments"] = False
    return {
      "schema_version": "0.1", "mode": "READ_ONLY", "physical_actuation_authorized": False,
      "qualification_status": bands.get("status", "MISSING"),
      "records": results, "counts": dict(Counter(item["state"] for item in results)),
      "overall": "REQUIRES_HUMAN_REVIEW" if any(r["review_required"] for r in results)
                 else "NO_NEW_QC_ALERTS",
      "operator_note": "This report never authorizes feeding, stimulation, peptide, genomic or culture changes.",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--observations", required=True, type=Path)
    parser.add_argument("--bands", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    bands = json.loads(args.bands.read_text(encoding="utf-8"))
    with args.observations.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if not reader.fieldnames:
            parser.error("Observations CSV has no header")
        missing = REQUIRED - set(reader.fieldnames)
        if missing:
            parser.error("Missing columns: " + ", ".join(sorted(missing)))
        rows = list(reader)
    result = check(rows, bands)
    output = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    # 0 means no new QC alerts, NOT authorization to run a biological study.
    return 1 if result["overall"] == "REQUIRES_HUMAN_REVIEW" else 0


if __name__ == "__main__":
    sys.exit(main())
