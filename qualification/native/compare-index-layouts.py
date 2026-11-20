#!/usr/bin/env python3
"""Run paired, opposite-order scale trials on one volume with pinned input bytes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

LAYOUTS = {"A": ("node-files", "native-sealed-avl"),
           "B": ("packed-pages", "native-packed-pages/1")}
COUNTERS = ("observations", "epochs", "distinct_local_source_uids",
            "receiver_observations", "receiver_expired_observations")


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def pin(source, target):
    with source.open("rb") as inp:
        data = inp.read(16385)
    if len(data) > 16384:
        raise ValueError(f"input exceeds 16 KiB: {source}")
    with target.open("xb") as out:
        out.write(data)
    return hashlib.sha256(data).hexdigest()


def save(path, value):
    tmp = path.with_suffix(".next")
    with tmp.open("w", encoding="utf-8", newline="\n") as out:
        json.dump(value, out, indent=2, sort_keys=True, allow_nan=False)
        out.write("\n")
    os.replace(tmp, path)


def measurements(report):
    checkpoints = []
    for point in report["checkpoints"]:
        compaction = point["compaction"]
        if not (compaction["coexistence_measured"] and compaction["activated"]
                and compaction["cleanup_complete"] and point["restart_verified"]
                and point["identity_fences_verified"] and point["exact_retry_verified"]):
            raise ValueError("incomplete compaction, recovery or coexistence evidence")
        after = point["disk_after"]
        checkpoints.append({
            "observations": point["observations"],
            "steady_files": after["files"], "steady_directories": after["directories"],
            "coexistence_files": compaction["coexistence_files"],
            "coexistence_directories": compaction["coexistence_directories"],
            "old_index_files": compaction["coexistence_old_index_files"],
            "new_index_files": compaction["coexistence_new_index_files"],
            "allocated_bytes": after["allocated_bytes"] if after["allocated_bytes_available"] else None,
            "directory_bytes": after["directory_allocated_bytes"] if after["directory_allocation_available"] else None,
            "reopen_ns": point["reopen_ns"], "compaction_ns": point["compaction_ns"],
            "lookup_p99_upper_ns": {k: point[k]["latency"]["p99_upper_ns"]
                                    for k in ("oldest", "middle", "recent")},
        })
    controls = report["controls"]
    if not checkpoints or not all(controls[k] for k in ("protected_root_verified", "quota_rejection_verified", "maintenance_reserve_verified")):
        raise ValueError("report lacks completed checkpoints or controls")
    return {
        "workload": {k: report["workload"][k] for k in COUNTERS},
        "commit": report["commit_acceptance"], "counts": report["exact_threshold_counts"],
        "wall_ns": report["workload"]["wall_ns"],
        "peak_resident_bytes": report["resources"]["peak_resident_bytes"],
        "checkpoints": checkpoints,
    }


def run_case(exe, output, profile, device, manifest, index, letter):
    layout, backend = LAYOUTS[letter]
    name = f"{index}-{layout}"
    dest = output / name
    case = {"directory": name, "layout": layout, "usable": False}
    try:
        if sha(exe) != manifest["executable_sha256"]:
            raise ValueError("executable changed before trial")
        with (output / (name + ".stdout")).open("xb") as stdout, (output / (name + ".stderr")).open("xb") as stderr:
            cmd = [str(exe), "scale", str(profile), str(dest), str(device), "--index-layout", layout]
            case["returncode"] = subprocess.run(cmd, stdout=stdout, stderr=stderr, check=False).returncode
        case["report_sha256"] = sha(dest / "report.json")
        report = json.loads((dest / "report.json").read_bytes())
        case["violations"] = report["violations"]
        case["build_identity"] = report["environment"]["build_identity"]
        case["admission"] = report["admission"]
        if (report["index_layout"] != layout or dest.stat().st_dev != output.stat().st_dev
                or sha(exe) != manifest["executable_sha256"]):
            raise ValueError("trial disagrees with declared layout, volume or executable")
        for file, key, report_key in ((profile, "profile_sha256", "profile_sha256"),
                                      (device, "device_sha256", "device_sha256")):
            if not (sha(dest / file.name) == sha(file) == manifest[key] == report[report_key]):
                raise ValueError("trial input differs from admitted bytes")
        if (case["returncode"] == 0) != report["passed"]:
            raise ValueError("exit code disagrees with report")
        if report["index_backend"] == backend and report["checkpoints"]:
            case["metrics"] = measurements(report)
        case["usable"] = report["passed"] and "metrics" in case
    except (OSError, ValueError, KeyError, TypeError) as exc:
        case["error"] = str(exc)
    return case


def compare(first, second):
    a, b = sorted((first, second), key=lambda c: c["layout"] != "node-files")
    pair = {"baseline": a["directory"], "candidate": b["directory"], "passed": False}
    if not a["usable"] or not b["usable"]:
        pair["reason"] = "one or both trials failed or lack verified measurements"
        return pair
    old, new = a["metrics"], b["metrics"]
    old_points, new_points = old["checkpoints"], new["checkpoints"]
    if (a["build_identity"] != b["build_identity"] or old["workload"] != new["workload"]
            or [p["observations"] for p in old_points] != [p["observations"] for p in new_points]
            or any(old["counts"][k] != new["counts"][k] for k in ("commit_samples", "lookup_samples"))):
        pair["reason"] = "build, admitted workload or sample counts differ"
        return pair
    diffs = []
    for older, newer in zip(old_points, new_points):
        diffs.append({
            "observations": older["observations"],
            "fewer_steady_files": older["steady_files"] - newer["steady_files"],
            "fewer_coexisting_file_nodes": (older["coexistence_files"] + older["coexistence_directories"]
                                             - newer["coexistence_files"] - newer["coexistence_directories"]),
        })
    pair["checkpoints"] = diffs
    pair["additional_over_budget_commits"] = new["counts"]["commit_over_budget"] - old["counts"]["commit_over_budget"]
    pair["passed"] = (pair["additional_over_budget_commits"] <= 0 and
                      all(d["fewer_steady_files"] > 0 and d["fewer_coexisting_file_nodes"] > 0 for d in diffs))
    return pair


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("profile", type=Path)
    parser.add_argument("device", type=Path)
    parser.add_argument("output", type=Path, help="new directory on the measurement volume")
    parser.add_argument("--order", choices=("ABBA", "BAAB"), default="ABBA")
    args = parser.parse_args()
    exe, output = args.executable.resolve(strict=True), args.output.resolve()
    output.mkdir()
    profile, device = output / "profile.cevqualification", output / "device.cevdevice"
    manifest = {"schema": "anaiero.index-comparison/1", "order": args.order,
                "executable_sha256": sha(exe), "profile_sha256": pin(args.profile, profile),
                "device_sha256": pin(args.device, device), "runs": [], "pairs": [], "complete": False,
                "file_node_scope": "regular files plus directories at verified replacement before cleanup",
                "criterion": "Both native gates pass in each pair, fewer steady files and fewer coexisting nodes at each checkpoint, no additional commits over budget"}
    save(output / "comparison.json", manifest)
    for i, letter in enumerate(args.order, 1):
        print(f"Running trial {i}/4 ({LAYOUTS[letter][0]})", flush=True)
        manifest["runs"].append(run_case(exe, output, profile, device, manifest, i, letter))
        if i % 2 == 0:
            manifest["pairs"].append(compare(*manifest["runs"][-2:]))
        save(output / "comparison.json", manifest)
    manifest["passed"] = all(p["passed"] for p in manifest["pairs"])
    manifest["complete"] = True
    save(output / "comparison.json", manifest)
    print(f'{output / "comparison.json"} (passed={manifest["passed"]})')
    return 0 if manifest["passed"] else 2


if __name__ == "__main__":
    sys.exit(main())
