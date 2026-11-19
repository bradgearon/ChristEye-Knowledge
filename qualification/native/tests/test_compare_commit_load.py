"""Regression checks for experiment integrity around the separately tested native CLI."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "compare-commit-load.py"
SPEC = importlib.util.spec_from_file_location("commit_load", SCRIPT)
comparison = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(comparison)


class LoadComparisonTest(unittest.TestCase):
    def test_preserves_failed_first_condition_and_confirms_load(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            executable = root / "pinned-test-executable"
            executable.write_bytes(b"an executable fixture")
            profile = root / "profile"
            profile.write_bytes(b"ANAIEROQUALIFICATION/2\n")
            device = root / "device"
            device.write_bytes(b"native test device")
            run = root / "comparison"
            conditions = []

            def native(command, stdout, stderr, check):
                target = Path(command[3])
                target.mkdir()
                condition = target.parent.name
                conditions.append(condition)
                # Observe an actual update in the fixed-size sibling file
                # while the controlled condition's native invocation is active.
                if condition == "controlled-load":
                    loadfile = target.parent / "background-io.bin"
                    self.assertEqual(loadfile.stat().st_size, comparison.FILE_BLOCKS * comparison.BLOCK_BYTES)
                    initial = comparison.digest(loadfile)
                    deadline = time.monotonic() + 5
                    while comparison.digest(loadfile) == initial:
                        if time.monotonic() > deadline:
                            self.fail("writer made no observable progress during invocation")
                        time.sleep(0.025)
                accepted = condition == "controlled-load"
                for name, origin in (("profile.cevqualification", profile), ("device.cevdevice", device)):
                    (target / name).write_bytes(origin.read_bytes())
                report = {
                    "passed": accepted, "profile_sha256": comparison.digest(profile),
                    "device_declaration_sha256": comparison.digest(device),
                    "index_layout": "node-files", "index_backend": "native-sealed-avl",
                    "device_declaration": {"device_class": "shared"},
                    "environment": {"build_identity": "unit-test-build"},
                    "workload": {"observations": 10}, "commit_threshold": {"passed": accepted},
                    "slow_commits": {"above_limit_omitted": 0}, "audit": {"passed": True},
                }
                data = json.dumps(report).encode() + b"\n"
                (target / "report.json").write_bytes(data)
                stdout.write(data.replace(b"\n", b"\r\n"))
                return subprocess.CompletedProcess(command, 0 if accepted else 2)

            argv = [str(SCRIPT), str(executable), str(profile), str(device), str(run)]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(comparison.subprocess, "run", side_effect=native), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(comparison.main(), 2)
                saved = (run / "comparison.json").read_bytes()
                with self.assertRaises(FileExistsError):
                    comparison.main()
            self.assertEqual(conditions, ["idle", "controlled-load"])
            self.assertEqual((run / "comparison.json").read_bytes(), saved)
            result = json.loads(saved)
            self.assertTrue(result["comparable"])
            self.assertFalse(result["all_qualification_gates_passed"])
            baseline, loaded = result["conditions"]
            self.assertEqual(baseline["flushes_during_native"], 0)
            self.assertGreater(loaded["flushes_during_native"], 0)
            self.assertLessEqual(loaded["writer"]["started_ns"], loaded["native_started_ns"])
            self.assertGreaterEqual(loaded["writer"]["stopped_ns"], loaded["native_stopped_ns"])
            self.assertEqual(loaded["writer"]["bytes_written"], loaded["writer"]["completed_flushes"] * comparison.BLOCK_BYTES)
            self.assertFalse(any(run.rglob("background-io.bin")))


if __name__ == "__main__":
    unittest.main()
