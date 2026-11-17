#!/usr/bin/env python3
"""Optional external live numerical-solver sidecar for the native Anaiero controller.

The 'synthetic' mode is a dependency-free contract test. The 'neuron' mode
runs a small separately installed NEURON model using h.fadvance. None of
this controls a physical instrument or validates cell-specific biology.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def make_solver(backend: str):
    if backend == "synthetic":
        state = {"activity": 0}

        def respond(channel: str, input_proxy: int, dt_ns: int):
            if channel == "M":
                return 75
            state["activity"] = (3 * state["activity"] + input_proxy) // 4
            return state["activity"]

        return respond

    if backend != "neuron":
        raise ValueError("unknown solver")
    try:
        from neuron import h
    except ImportError as exc:
        raise RuntimeError("NEURON is not installed; use --backend synthetic") from exc

    soma = h.Section(name="anaiero_optional_live_soma")
    soma.L = 20
    soma.diam = 20
    soma.insert("pas")
    soma.g_pas = 0.001
    soma.e_pas = -65
    stimulus = h.IClamp(soma(0.5))
    stimulus.delay = 0
    stimulus.dur = 1000000
    stimulus.amp = 0
    h.dt = 0.025
    h.finitialize(-65)

    def respond(channel: str, input_proxy: int, dt_ns: int):
        if channel == "M":
            # Synthetic, NOT PhysiCell. Real environment step adapters would
            # supply a separate, source-qualified model and unit mapping.
            return 75
        h.dt = dt_ns / 1_000_000  # nanoseconds -> milliseconds in NEURON
        stimulus.amp = 0.02 * input_proxy / 1000  # numerical model input only
        h.fadvance()
        membrane_mv = float(soma(0.5).v)
        # Illustrative transform: NOT calibrated neural activity or learning.
        return max(0, min(1000, round(500 + 8 * (membrane_mv + 65))))

    return respond


def run(args: argparse.Namespace) -> int:
    solver = make_solver(args.backend)
    source_bytes = Path(__file__).read_bytes()
    model_sha = hashlib.sha256(source_bytes + args.backend.encode("ascii")).hexdigest()
    process = subprocess.Popen(
        [
            str(args.anaiero), "science", "live", str(args.atlas),
            str(args.store), str(args.epoch), model_sha, args.backend,
        ],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, encoding="utf-8", bufsize=1,
    )
    assert process.stdin is not None and process.stdout is not None
    seen = 0
    result = None
    try:
        for line in process.stdout:
            line = line.rstrip("\n")
            if line.startswith("RESULT "):
                result = json.loads(line[7:])
                break
            tokens = line.split()
            if len(tokens) != 7 or tokens[0] != "STEP" or tokens[1] not in ("E", "M"):
                raise RuntimeError(f"invalid controller protocol frame: {line!r}")
            _, channel, epoch, sequence, dt_ns, pattern, declared_hash = tokens
            if int(epoch) != args.epoch or declared_hash != model_sha:
                raise RuntimeError("model identity or source epoch disagreement")
            count = int(sequence)
            duration = int(dt_ns)
            input_proxy = int(pattern)
            seen += 1
            feature = solver(channel, input_proxy, duration)
            if args.invalid_at and seen == args.invalid_at:
                # Negative-test mode: provoke the native fail-closed path.
                count += 1
            process.stdin.write(
                f"SAMPLE {channel} {epoch} {count} {int(sequence)*duration} "
                f"{feature} 0 {model_sha} OK\n"
            )
            process.stdin.flush()
    except (BrokenPipeError, ValueError, RuntimeError) as exc:
        process.kill()
        process.wait()
        print(f"live solver error: {exc}", file=sys.stderr)
        return 1
    finally:
        process.stdin.close()
    rc = process.wait(timeout=60)
    stderr = process.stderr.read() if process.stderr else ""
    if args.expect_failure:
        if rc == 0 or args.invalid_at == 0:
            print("expected failure was not detected", file=sys.stderr)
            return 1
        print(json.dumps({"expected_failure": True, "exchange_count": seen}))
        return 0
    if rc != 0 or result is None:
        print(f"native controller exited {rc}: {stderr}", file=sys.stderr)
        return 1
    if result["schema"] != "anaiero.science.live/1":
        raise RuntimeError("wrong native result schema")
    print(json.dumps({"external_backend": args.backend, **result},
                     sort_keys=True, separators=(",", ":")))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--anaiero", type=Path, required=True)
    parser.add_argument("--atlas", type=Path, required=True)
    parser.add_argument("--store", type=Path, required=True)
    parser.add_argument("--epoch", type=int, default=1)
    parser.add_argument("--backend", choices=("synthetic", "neuron"),
                        default="synthetic")
    parser.add_argument("--invalid-at", type=int, default=0, help=argparse.SUPPRESS)
    parser.add_argument("--expect-failure", action="store_true", help=argparse.SUPPRESS)
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
