#!/usr/bin/env python3
"""Replay a pinned native qualification with and without a bounded filesystem writer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time


BLOCK_BYTES = 64 * 1024
FILE_BLOCKS = 64
PERIOD_NS = 20_000_000
DISK_STAT_FIELDS = (
    "reads", "reads_merged", "sectors_read", "read_ms", "writes", "writes_merged",
    "sectors_written", "write_ms", "io_in_flight", "busy_ms", "weighted_io_ms",
    "discards", "discards_merged", "sectors_discarded", "discard_ms", "flushes", "flush_ms",
)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save_json(path, value):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write("\n")


def snapshot(root):
    # Values are volume- or host-wide observations. Overlay and virtual volumes
    # can legitimately have no underlying block-device statistics.
    device = root.stat().st_dev
    result = {
        "unix_ns": time.time_ns(), "filesystem_device": device,
        "free_bytes": shutil.disk_usage(root).free, "free_file_nodes": None,
        "block_device_counters": None, "io_pressure": None, "cpu_pressure": None,
    }
    if hasattr(os, "statvfs"):
        fs = os.statvfs(root)
        if fs.f_files:
            result["free_file_nodes"] = fs.f_favail
    if sys.platform.startswith("linux"):
        stat = Path("/sys/dev/block") / f"{os.major(device)}:{os.minor(device)}" / "stat"
        try:
            result["block_device_counters"] = dict(zip(DISK_STAT_FIELDS, map(int, stat.read_text().split())))
        except (OSError, ValueError):
            pass
        for kind in ("cpu", "io"):
            try:
                result[f"{kind}_pressure"] = (Path("/proc/pressure") / kind).read_text().splitlines()
            except OSError:
                pass
    return result


class ProbeWriter:
    """Own a new fixed-size file, initialize it identically for both conditions."""

    def __init__(self, path):
        self.path = path
        self.handle = None
        self.stop = threading.Event()
        self.ready = threading.Event()
        self.error = None
        self.thread = None
        self.stats = {"completed_flushes": 0, "bytes_written": 0, "flush_total_ns": 0,
                      "flush_max_ns": 0, "period_overruns": 0, "started_ns": None, "stopped_ns": None}
        self.blocks = [
            hashlib.shake_256(f"anaiero.controlled-io/1/{i}".encode("ascii")).digest(BLOCK_BYTES)
            for i in range(2 * FILE_BLOCKS)
        ]

    def __enter__(self):
        self.handle = self.path.open("x+b", buffering=0)
        try:
            for block in self.blocks[:FILE_BLOCKS]:
                self.write(block)
            os.fsync(self.handle.fileno())
        except BaseException:
            self.handle.close()
            self.path.unlink()
            raise
        return self

    def write(self, block):
        view = memoryview(block)
        while view:
            written = self.handle.write(view)
            if not written:
                raise OSError("short write preparing background I/O")
            view = view[written:]

    def loop(self):
        sequence = FILE_BLOCKS
        self.stats["started_ns"] = time.monotonic_ns()
        try:
            while not self.stop.is_set():
                start = time.monotonic_ns()
                self.handle.seek((sequence % FILE_BLOCKS) * BLOCK_BYTES)
                self.write(self.blocks[sequence % len(self.blocks)])
                sync_start = time.monotonic_ns()
                os.fsync(self.handle.fileno())
                end = time.monotonic_ns()
                latency = end - sync_start
                self.stats["completed_flushes"] += 1
                self.stats["bytes_written"] += BLOCK_BYTES
                self.stats["flush_total_ns"] += latency
                self.stats["flush_max_ns"] = max(self.stats["flush_max_ns"], latency)
                self.ready.set()
                sequence += 1
                slack = PERIOD_NS - (end - start)
                if slack <= 0:
                    self.stats["period_overruns"] += 1
                else:
                    self.stop.wait(slack / 1e9)
        except Exception as error:
            self.error = str(error)
        finally:
            self.stats["stopped_ns"] = time.monotonic_ns()
            self.ready.set()

    def start(self):
        self.thread = threading.Thread(target=self.loop, name="qualification-fsync")
        self.thread.start()
        self.ready.wait()
        if self.error:
            raise OSError(self.error)

    def __exit__(self, *args):
        self.stop.set()
        if self.thread:
            self.thread.join()
        self.handle.close()
        self.path.unlink()


def attempt(root, executable, pins, layout, condition):
    stage = root / condition
    stage.mkdir()
    result = {"condition": condition, "complete": False, "qualification_passed": False,
              "returncode": None, "issues": [], "before": snapshot(stage)}
    try:
        if digest(executable) != pins["executable"]:
            raise ValueError("executable changed before this attempt")
        with ProbeWriter(stage / "background-io.bin") as writer:
            if condition == "controlled-load":
                writer.start()
            command = [str(executable), "run", str(root / "profile.cevqualification"),
                       str(stage / "native"), str(root / "device.cevdevice"), "--index-layout", layout]
            with (stage / "stdout.json").open("xb") as stdout, (stage / "stderr.log").open("xb") as stderr:
                result["native_started_ns"] = time.monotonic_ns()
                starting_flush_count = writer.stats["completed_flushes"]
                child = subprocess.run(command, stdout=stdout, stderr=stderr, check=False)
                result["flushes_during_native"] = writer.stats["completed_flushes"] - starting_flush_count
                result["native_stopped_ns"] = time.monotonic_ns()
                result["returncode"] = child.returncode
        result["writer"] = writer.stats
        if writer.error:
            result["issues"].append(f"background writer failed: {writer.error}")
        if condition == "controlled-load" and result["flushes_during_native"] == 0:
            result["issues"].append("no controlled flush completed during the native invocation")
        if digest(executable) != pins["executable"]:
            result["issues"].append("executable changed during this attempt")
        native = stage / "native"
        if native.stat().st_dev != stage.stat().st_dev:
            raise ValueError("load file and native output are on different filesystems")
        report_path = native / "report.json"
        report = json.loads(report_path.read_bytes())
        result["report_sha256"] = digest(report_path)
        # The Windows text console may translate the report's final LF to
        # CRLF. The native artifact remains byte exact on both platforms.
        stdout_path = stage / "stdout.json"
        result["stdout_sha256"] = digest(stdout_path)
        if stdout_path.read_bytes().replace(b"\r\n", b"\n") != report_path.read_bytes().replace(b"\r\n", b"\n"):
            result["issues"].append("stdout and retained native report differ")
        if (report["profile_sha256"] != pins["profile"] or
                digest(native / "profile.cevqualification") != pins["profile"] or
                report["device_declaration_sha256"] != pins["device"] or
                digest(native / "device.cevdevice") != pins["device"]):
            result["issues"].append("native inputs differ from predeclared hashes")
        expected_backend = "native-packed-pages/1" if layout == "packed-pages" else "native-sealed-avl"
        if report["index_layout"] != layout or (report["audit"]["passed"] and report["index_backend"] != expected_backend):
            result["issues"].append("native index layout differs from the predeclared layout")
        if report["passed"] != (child.returncode == 0):
            result["issues"].append("native exit status disagrees with the retained report")
        result["build_identity"] = report["environment"]["build_identity"]
        result["device_class"] = report["device_declaration"]["device_class"]
        result["observations"] = report["workload"]["observations"]
        result["throughput_commits"] = report["commit_threshold"]
        result["above_limit_omitted"] = report["slow_commits"]["above_limit_omitted"]
        result["slowest_commits_path"] = f"{condition}/native/report.json#slow_commits"
        result["qualification_passed"] = report["passed"]
        result["audit_passed"] = report["audit"]["passed"]
        result["complete"] = bool(result["audit_passed"] and not result["issues"])
    except (KeyError, OSError, ValueError) as error:
        result["issues"].append(str(error))
    result["after"] = snapshot(stage)
    save_json(stage / "condition.json", result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("profile", type=Path)
    parser.add_argument("device", type=Path)
    parser.add_argument("new_directory", type=Path)
    parser.add_argument("--layout", choices=["node-files", "packed-pages"], default="node-files")
    parser.add_argument("--order", choices=["idle-first", "load-first"], default="idle-first")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    root = args.new_directory.absolute()
    # Validate and pin both text inputs before spending disk space or starting a worker.
    inputs = {}
    for key, source in (("profile", args.profile), ("device", args.device)):
        with source.open("rb") as stream:
            data = stream.read(16385)
        if not data or len(data) > 16384:
            raise ValueError(f"invalid {key} size")
        inputs[key] = data
    if inputs["profile"].splitlines()[0] != b"ANAIEROQUALIFICATION/2":
        raise ValueError("a version 2 exact-gate qualification profile is required")
    pins = {k: hashlib.sha256(v).hexdigest() for k, v in inputs.items()}
    pins["executable"] = digest(executable)
    root.mkdir()
    for key, filename in (("profile", "profile.cevqualification"), ("device", "device.cevdevice")):
        with (root / filename).open("xb") as stream:
            stream.write(inputs[key])
    sequence = ["idle", "controlled-load"] if args.order == "idle-first" else ["controlled-load", "idle"]
    plan = {
        "schema": "anaiero.commit-load-comparison/1", "pins_sha256": pins,
        "executable_name": executable.name, "layout": args.layout, "sequence": sequence,
        "controlled_writer": {"block_bytes": BLOCK_BYTES, "extent_bytes": BLOCK_BYTES * FILE_BLOCKS,
                              "minimum_start_interval_ns": PERIOD_NS, "flush_every_write": True,
                              "pattern": "deterministic alternating sequential rings, one worker, no catch-up"},
        "idle_scope": "no harness-generated load during the native run; external activity and page cache are uncontrolled",
        "clock": "monotonic nanoseconds for intervals; before/after snapshots also include Unix time",
    }
    save_json(root / "plan.json", plan)
    results = [attempt(root, executable, pins, args.layout, condition) for condition in sequence]
    build_ids = {r.get("build_identity") for r in results}
    matched = all(r["complete"] for r in results) and len(build_ids) == 1
    passed = matched and all(r["qualification_passed"] for r in results)
    summary = {"schema": "anaiero.commit-load-result/1", "plan_sha256": digest(root / "plan.json"),
               "comparable": matched, "all_qualification_gates_passed": passed, "conditions": results}
    save_json(root / "comparison.json", summary)
    print(json.dumps({"directory": str(root), "comparable": matched, "all_qualification_gates_passed": passed}))
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
