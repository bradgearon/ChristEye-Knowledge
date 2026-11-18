# CE-SW-34 — Supervised scientific engine sessions

This batch makes an optional scientific solver an owned native session engine.
The native controller owns launch, framed I/O, deadlines, cancellation, process
termination, identity negotiation, checkpointing, and observation publication.
Python remains optional and outside the controller process.

## Process and engine contracts

`IProcessExchange` separates process mechanisms from solver decisions. The
Windows adapter uses overlapped named pipes, direct `CreateProcess` argument
passing, and a kill-on-close Job Object. The Linux adapter uses nonblocking
pipes, an owned process group, readiness waits, and direct executable launch.
The executable and argument vector are never sent through a shell.

One absolute deadline covers a request's write and response read, including
a provider that never reads stdin. Stderr drains concurrently into a bounded
tail. Cancellation wakes blocked I/O and terminates the owned process. A
timeout, process exit, malformed/oversized frame, or identity mismatch makes
the solver epoch unusable until it is reinitialized and restored; the runtime
does not guess how far an unacknowledged numerical calculation progressed.

`SupervisedSolverExchange` owns the versioned protocol. The handshake verifies
provider, model and behavior digests, source epoch, time step, quantity unit,
checkpoint schema, and maximum steps. The configuration loader pins model,
script arguments, and executable content. The outer session identity includes
the nested configuration identity, which the engine factory checks again
before construction.

`ScientificEngineService` maps this exchange to `ISimulationEngine`. Its
serialized checkpoint binds engine/source identity and configured input to a
sealed complete solver checkpoint. The snapshot hash is the SHA-256 of those
exact checkpoint bytes, satisfying the same catalog invariant as other
engines. Restore verifies the full numerical state and sampled quantity in a
fresh provider; it does not reconstruct state by replaying an unbounded history.

## What is modeled

The mandatory C++20 provider integrates an exact passive RC compartment. The
optional provider uses actual NEURON **9.0.1** and its `SaveState` APIs. Both
report modeled membrane voltage in microvolts with a declared simulated
nanosecond timebase. The session emits one `membrane_voltage` observation per
scientific snapshot. Participants, connections, and learning summaries are
unsupported, and no viability or energy-proxy evidence is manufactured.

The supplied model is a passive numerical example. Its numerical envelope is
not biological calibration, measurement uncertainty, or validation of neural
learning. This batch does not add an environmental model, a spiking neuron,
scientific candidate evaluation, or live scientific policy changes.

## Running it

The [scientific examples guide](../../examples/scientific/README.md) contains
complete native and optional NEURON commands, model parameters, equations,
identity fields, and qualification measurements. The native setup utility
writes a pinned solver configuration and a sibling segmented session file:

```sh
build/native/ce_scientific_config native \
  build/native/ce_scientific_solver_provider \
  examples/scientific/passive-membrane.cevmodel run/science/native.cevsolver
build/native/ce_colony_runtime session \
  run/science/native.cevsession run/science/state 40
```

Use platform-appropriate executable paths on Windows. Both commands are
native and need no Python installation. The existing `science live` stdio CLI
retains its caller-owned transport contract; owned process supervision is the
new scientific-session path.

A smaller build profile omits the legacy/demo CLI collection:

```sh
cmake -S native -B build/controller -DANAIERO_CONTROLLER_ONLY=ON \
  -DCE_WIND_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/controller --config Release --parallel 2
```

It builds the session controller, native provider, and configuration utility.
Windows and Linux CI build this profile without Python package discovery.

## Qualification and limits

Native failure tests cover exact-model comparison, complete checkpoint
restart, source/input/model/behavior/epoch mismatches, damaged seals, handshake
hangs, blocked request writes, hung/partial/malformed/oversized replies,
concurrent cancellation, process crashes, stderr floods, model tampering,
and restored-at-limit snapshots. Integrated tests publish scientific voltage
through the actual observation journal and Projector, recover the session,
and retain stable operation results.

The actual NEURON qualification uses 200 samples at 25,000 ns per step and
compares each with the independent analytic RC solution. The measured maximum
absolute difference was **7.60636701136 microvolts**, below its conservative
**41-microvolt numerical envelope**. A checkpoint at sample 40 restored into
a fresh NEURON process; the following 160 samples, full checkpoint bytes, and
state hashes matched exactly. The test writes `qualification.json` containing
the pinned identities and measurements, and CI uploads that report.

The NEURON qualification job runs on Linux. Mandatory native process and
analytic-provider tests run on Windows and Linux. A successful cross-compile
does not replace the Windows CI run.

| Resource | Bound |
| --- | --- |
| Request/response frame | 16 KiB |
| Retained stderr diagnostics | 8 KiB |
| Opaque provider checkpoint reply | 4 KiB |
| Arguments | 32; 4 KiB each; 16 KiB combined |
| Configurable exchange deadline | 1–60,000 ms |
| Session provider steps | At most 1,000,000 |
| Scientific live metric | Modeled membrane voltage, microvolts |

These are bounded operations on a general-purpose OS, with ordinary scheduler
latency. The process owner is not an operating-system security sandbox.
Checkpoint compatibility is pinned to model, behavior, provider, schema, and
configuration; migration to an unrelated solver build is not promised.
