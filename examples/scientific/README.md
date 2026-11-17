# Supervised scientific sessions

The native host can own a scientific solver process through the same
`ISimulationEngine` lifecycle as SW5 and SIM3. Windows uses overlapped pipes and
a kill-on-close Job Object; Linux uses nonblocking pipes and an owned process
group. Arguments are passed directly to the executable without a shell. One
absolute deadline covers each request write and response read. Cancellation
wakes a blocked exchange and kills the owned process. A timeout, crash,
malformed response, or identity mismatch poisons that solver epoch; it cannot
continue from an unknown numerical state.

The mandatory provider is a native C++20 exact passive RC model. The optional
provider uses NEURON **9.0.1** in a separate process. Neither provider supplies a
measured cell readout, biological calibration, spike model, or viability score.
The only published scientific summary is **modeled membrane voltage in
microvolts**. The host preserves its distinct metric and physical timebase.

## Native example

Build `ce_scientific_solver_provider`, `ce_scientific_config`, and the normal native
runtime. From the repository root, using the appropriate executable suffix on
Windows:

```sh
build/native/ce_scientific_config native \
  build/native/ce_scientific_solver_provider \
  examples/scientific/passive-membrane.cevmodel \
  run/science/native.cevsolver

build/native/ce_colony_runtime session \
  run/science/native.cevsession run/science/state 40
```

The setup utility writes a pinned `.cevsolver` file and a sibling
`CEVSESSION/2` configuration using segmented storage. It refuses to overwrite
existing outputs. Change the example build path to your CMake binary directory.
The core controller and this native setup path do not require Python.

The model manifest sets resting voltage to -65,000 microvolts, resistance to
79,577,472 ohms, time constant to 1,000,000 ns, and maximum injected current to
20 pA. The generated input is 1,000/1,000 of that maximum, with 25,000 ns steps.
The native service advances the exact solution for a constant input during
each step:

\[
V_{next}=V_{target}+(V_{old}-V_{target})e^{-\Delta t/\tau},\qquad
V_{target}=E+RI.
\]

It reports rounded microvolts with a one-microvolt numerical rounding envelope.
The provider checkpoint includes the full-precision voltage and step position.
There is no numerical state replay during recovery.

## Optional actual NEURON qualification

Install the optional external provider in a dedicated environment, then use
the same native setup utility:

```sh
python -m venv .venv-science
.venv-science/bin/python -m pip install -r tools/scientific/requirements-neuron.txt

build/native/ce_scientific_config neuron \
  .venv-science/bin/python tools/scientific/supervised_neuron_provider.py \
  examples/scientific/passive-membrane.cevmodel \
  run/science/neuron.cevsolver

build/native/ce_neuron_qualification_tests \
  .venv-science/bin/python tools/scientific/supervised_neuron_provider.py \
  examples/scientific/passive-membrane.cevmodel run/science/qualification
```

The optional qualification path is tested on Linux. The provider checks the
NEURON package version before accepting its handshake. It constructs one
20-by-20 micrometer passive compartment, derives conductance and capacitance
from the pinned RC manifest, uses a fixed step, and injects the configured
current through `IClamp`. Its conservative numerical error envelope is
`ceil(R * max_current_pA / 1,000,000 * step_ns / tau_ns) + 1` microvolts.
This describes discretization and output rounding; it does not estimate
biological or measurement uncertainty.

The qualification compares every sample with the independent analytic RC
solution and writes `qualification.json` with the model, behavior, script,
and configuration digests. A measured NEURON 9.0.1 run produced:

| Check | Measured result |
| --- | --- |
| Fixed step | 25,000 ns |
| Comparison samples | 200 |
| Maximum absolute analytic difference | 7.60636701136 microvolts |
| Declared numerical envelope | 41 microvolts |
| Restart point | Sample 40 |
| Exact continuation after a fresh process restore | 160 samples |
| Final full checkpoint bytes | Equal between original and restored processes |

NEURON's actual `SaveState.save/fwrite/fread/restore` APIs save and restore its
numerical state. The fixed model construction order is part of the behavior
identity; these checkpoints do not promise migration to another solver build
or model structure. See the [NEURON 9.0.1 SaveState reference](https://www.neuronsimulator.org/en/9.0.1/progref/simctrl/savstate.html)
and [installation instructions](https://www.neuronsimulator.org/en/9.0.1/install/install_instructions.html).

## Identity, capabilities, and bounds

The handshake pins model SHA-256, behavior SHA-256, provider, source epoch,
step duration, quantity unit, checkpoint schema, and maximum steps. The
configuration loader verifies the model and any script arguments by digest
and includes the executable bytes in the configuration identity. It keeps
the executable launch path so virtual environments retain their runtime
selection. Session recovery also checks that nested configuration identity.

Checkpoint-capable providers bind their complete opaque checkpoint into the
engine state hash. The checkpoint wrapper also binds the latest sampled
sequence, physical time, quantity, uncertainty, model, behavior, and epoch.
Restore requires a fresh initialized provider and verifies the restored
sample. These providers advertise checkpoint/restore; they do not advertise
candidate evaluation, isolated experimental forks, or live policy application.

Protocol frames are bounded to 16 KiB, diagnostics to an 8 KiB tail, and opaque
provider checkpoint replies to 4 KiB. Configuration arguments are bounded to
32 entries, 4 KiB per entry, and 16 KiB total. Session providers admit at most
1,000,000 steps and 1–60,000 ms exchange deadlines. These are bounded host
operations, not hard real-time guarantees or a security sandbox.

Mandatory native tests include exact model comparison, fresh-process restart,
model/behavior/epoch mismatch, damaged checkpoint seals, handshake hangs,
hung and partial replies, malformed and oversized frames, cancellation,
process crashes, stderr flooding, and changed model files. The preexisting
`science live` stdio CLI remains available with its original caller-owned
transport contract; use supervised scientific sessions for owned process
lifecycle and bounded I/O.
