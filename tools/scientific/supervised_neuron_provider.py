#!/usr/bin/env python3
"""Pinned external NEURON provider for one passive electrical compartment.

Only modeled membrane voltage is qualified. This is neither a measured cell
readout nor a biological calibration. The controller remains native C++20.
SaveState is bound to the exact model/behavior/provider by the native protocol.
Reference: https://www.neuronsimulator.org/en/9.0.1/progref/simctrl/savstate.html
"""
from __future__ import annotations

import base64
import hashlib
import math
import pathlib
import sys
import tempfile

import neuron
from neuron import h

VERSION = "9.0.1"
PROVIDER = "neuron-passive-9.0.1"
BEHAVIOR = (
    "anaiero.neuron-passive/1;neuron=9.0.1;section=L20,diam20,nseg1;"
    "g_pas=1/(R*area_cm2);cm=tau_ns*g_pas/1000;e_pas=rest_uv/1000;"
    "IClamp.amp=max_current_pa*input/1000000;fixed-dt;sample_uv=round(v*1000);"
    "uncertainty_uv=ceil(R*max_current_pa/1000000*step_ns/tau_ns)+1;checkpoint=SaveState"
)
MAX_STEPS = 1_000_000
MAX_FRAME = 16_384


def bounded_model(path: str) -> tuple[dict[str, int], str]:
    with open(path, "rb") as source:
        raw = source.read(4097)
    if len(raw) > 4096:
        raise ValueError("model size")
    fields = raw.decode("ascii").split()
    if len(fields) != 10 or fields[0] != "CEPASSIVE/1" or fields[-1] != "end":
        raise ValueError("model format")
    names = ("rest_microvolt", "tau_ns", "resistance_ohm", "max_current_picoamp")
    if fields[1:9:2] != list(names):
        raise ValueError("model fields")
    model = {name: int(fields[2 + i * 2]) for i, name in enumerate(names)}
    if not (-1_000_000 <= model["rest_microvolt"] <= 1_000_000
            and 1 <= model["tau_ns"] <= 1_000_000_000_000
            and 1 <= model["resistance_ohm"] <= 10_000_000_000
            and 0 <= model["max_current_picoamp"] <= 1_000_000):
        raise ValueError("model bounds")
    return model, hashlib.sha256(raw).hexdigest()


def read_request() -> list[str] | None:
    raw = sys.stdin.buffer.readline(MAX_FRAME + 1)
    if not raw:
        return None
    if len(raw) > MAX_FRAME or not raw.endswith(b"\n") or any(x < 32 or x > 126 for x in raw[:-1]):
        raise ValueError("request frame")
    return raw.decode("ascii").split()


def reply(text: str) -> None:
    if len(text) >= MAX_FRAME:
        raise ValueError("reply frame")
    print(text, flush=True)


def run(model_path: str) -> None:
    if neuron.__version__ != VERSION:
        raise ValueError(f"requires NEURON {VERSION}, got {neuron.__version__}")
    model, model_hash = bounded_model(model_path)
    behavior_hash = hashlib.sha256(BEHAVIOR.encode("ascii")).hexdigest()
    hello = read_request()
    if hello is None or len(hello) != 6 or hello[:2] != ["HELLO", "1"] or hello[2:4] != [model_hash, behavior_hash]:
        raise ValueError("identity handshake")
    epoch, step_ns = int(hello[4]), int(hello[5])
    if not (1 <= epoch < 2**64 and 1 <= step_ns <= 1_000_000_000):
        raise ValueError("timebase")

    # Fixed structure and construction order are part of this provider version.
    # SaveState snapshots cannot be migrated to a different model structure.
    h.load_file("stdrun.hoc")
    section = h.Section(name="anaiero_passive")
    section.L, section.diam, section.nseg = 20, 20, 1
    section.insert("pas")
    area_cm2 = math.pi * 20 * 20 * 1e-8
    conductance = 1 / (model["resistance_ohm"] * area_cm2)
    section.g_pas = conductance
    section.cm = model["tau_ns"] * conductance / 1000
    section.e_pas = model["rest_microvolt"] / 1000
    clamp = h.IClamp(section(0.5))
    clamp.delay, clamp.dur, clamp.amp = 0, 1e12, 0
    h.CVode().active(0)
    h.secondorder = 0
    h.dt = step_ns / 1_000_000
    h.finitialize(model["rest_microvolt"] / 1000)
    gain_uv = model["resistance_ohm"] * model["max_current_picoamp"] / 1_000_000
    # Conservative numerical discretization + output rounding envelope for
    # the stable passive RC system with bounded nonnegative piecewise current.
    # This does not express biological or measurement uncertainty.
    uncertainty = math.ceil(gain_uv * step_ns / model["tau_ns"]) + 1
    if uncertainty >= 2**31:
        raise ValueError("numerical bound too large")
    sequence, current_input = 0, 0

    def sample() -> None:
        voltage = section(0.5).v * 1000
        if not math.isfinite(voltage) or not (-2**31 <= voltage < 2**31):
            raise ValueError("voltage bound")
        if not math.isclose(h.t, sequence * step_ns / 1_000_000, rel_tol=1e-8, abs_tol=1e-8):
            raise ValueError("solver clock drift")
        reply(f"SAMPLE {sequence} {sequence * step_ns} {round(voltage)} {uncertainty}")

    reply(f"READY 1 {model_hash} {behavior_hash} {epoch} {step_ns} 2 1 {MAX_STEPS} {PROVIDER}")
    with tempfile.TemporaryDirectory(prefix="anaiero-neuron-") as temporary:
        state_path = pathlib.Path(temporary) / "state.bin"
        while True:
            request = read_request()
            if request is None:
                break
            if request == ["LATEST"]:
                sample()
            elif len(request) == 4 and request[0] == "STEP":
                got_epoch, next_sequence, value = map(int, request[1:])
                if got_epoch != epoch or next_sequence != sequence + 1 or next_sequence > MAX_STEPS or not 0 <= value <= 1000:
                    raise ValueError("step identity")
                current_input = value
                clamp.amp = model["max_current_picoamp"] * value / 1_000_000
                h.fadvance()
                sequence = next_sequence
                sample()
            elif request == ["CHECKPOINT"]:
                saved = h.SaveState()
                saved.save()
                saved.fwrite(h.File(str(state_path)))
                with state_path.open("rb") as source:
                    binary = source.read(2049)
                if not binary or len(binary) > 2048:
                    raise ValueError("checkpoint size")
                encoded = base64.b64encode(binary).decode("ascii")
                reply(f"STATE {epoch} {step_ns} {sequence} {current_input} {encoded}")
            elif len(request) == 6 and request[0] == "RESTORE":
                got_epoch, got_step, got_sequence, value = map(int, request[1:5])
                if sequence != 0 or got_epoch != epoch or got_step != step_ns or not 0 <= got_sequence <= MAX_STEPS or not 0 <= value <= 1000:
                    raise ValueError("restore identity")
                binary = base64.b64decode(request[5], validate=True)
                if not binary or len(binary) > 2048:
                    raise ValueError("restore size")
                state_path.write_bytes(binary)
                saved = h.SaveState()
                saved.fread(h.File(str(state_path)))
                saved.restore()
                sequence, current_input = got_sequence, value
                clamp.amp = model["max_current_picoamp"] * value / 1_000_000
                sample()
            else:
                raise ValueError("unsupported request")


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("usage: supervised_neuron_provider.py model.cevmodel")
        run(sys.argv[1])
    except Exception as failure:
        print(f"neuron provider failed: {failure}", file=sys.stderr, flush=True)
        sys.exit(3)
