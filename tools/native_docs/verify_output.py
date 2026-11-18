#!/usr/bin/env python3
"""Check generated references for broken links, source tampering and payload budgets."""
import argparse
import hashlib
import json
from pathlib import Path


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def verify(site):
    versions = load(site / "versions.json")
    if versions.get("schema") != 2 or not versions.get("versions"):
        raise ValueError("No valid v2 version manifest")
    if not (site / "legacy.html").is_file():
        raise ValueError("Previous reference must remain available")
    for v in versions["versions"]:
        raw_path = site / v["index"]
        if not raw_path.is_file() or raw_path.stat().st_size > 2500000:
            raise ValueError("Missing or oversized search index: " + str(raw_path))
        index = load(raw_path)
        symbols, files = index["symbols"], index["files"]
        if len(symbols) != v["symbols"] or len(files) != v["headers"]:
            raise ValueError("Counts differ from source manifest")
        sym_ids = {s["id"] for s in symbols}
        file_ids = {f["id"] for f in files}
        if len(sym_ids) != len(symbols) or len(file_ids) != len(files):
            raise ValueError("Duplicated stable identifiers")
        buckets = {}
        for symbol in symbols:
            if symbol["fileId"] not in file_ids:
                raise ValueError("Symbol has no source file: " + symbol["qualified"])
            if symbol.get("parentId") and symbol["parentId"] not in sym_ids:
                raise ValueError("Broken parent symbol: " + symbol["qualified"])
            bucket = symbol["id"][2:4]
            if bucket not in buckets:
                buckets[bucket] = load(site / v["bucketPrefix"] / (bucket + ".json"))
            if symbol["id"] not in buckets[bucket]:
                raise ValueError("Missing detail for " + symbol["id"])
        for file in files:
            detail = load(site / v["filesPrefix"] / (file["id"] + ".json"))
            if hashlib.sha256(detail["source"].encode("utf-8")).hexdigest() != file["sha256"]:
                raise ValueError("Source checksum disagrees with published index")
        print(v["version"], len(symbols), "symbols", len(files), "files, integrity OK")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--site", required=True, type=Path)
    a = p.parse_args()
    verify(a.site)
