# Mixed native host qualification

`ce_native_qualification mixed` executes the five compiled engine families through
`NativeSession` and `NativeContinuousHost`. It measures the actual asynchronous
commit worker, continuous owner, canonical storage lane and orderly recovery path.
The full distribution exposes the same command as `anaiero qualify mixed`.
There is no Python, external numerical solver, IPC or TLS requirement in this
harness. The remote mesh and subprocess service have separate integration tests.

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --target ce_native_qualification --parallel 3
build/native/ce_native_qualification mixed qualification/native/mixed-ci.cevqualification examples/runtime/modular-native.cevsession build/mixed-ci
```

For a Visual Studio build, use `build/native/Release/ce_native_qualification.exe`.
Quote paths that contain spaces. The output directory must not exist, including
for failed earlier runs. Choose a new directory to preserve each attempt.

## Workload and pinned inputs

The strict `ANAIEROMIXEDQUALIFICATION/1` profile contains the entire workload and
all operating budgets. Missing, duplicate, unknown and noncanonical numeric
fields are rejected before creating an output directory. The harness requires
exactly one of each current compiled family and the reference cadences:

| Engine | Cadence | Expected behavior |
|---|---:|---|
| SW5 cells | Every cycle | Original cell input and model bytes execute through the registered module |
| SIM3 colonies | Every second cycle | Original colony configuration executes within the supported finite horizon |
| VLM plan | Every cycle until complete | The admitted finite program completes and stays completed across restart |
| Passive voltage | Every cycle | Native numerical module retains its original simulated nanosecond clock |
| Scalar environment | Every cycle except the first two | A durable pause, exact operation retry and resume exercise the control path |

The session file supplies the actual model, source, epoch, configuration and
input identities. The executable retains the original session text and the
complete encoded `session.definition`, including admitted model and asset bytes.
The report hashes both forms, the exact profile, every engine configuration and
input, and the compiled source/build identity. It records each final engine
identity, tick, revision, phase and state digest. Copying only the original
session text would not retain its relative-path assets; the encoded definition
and canonical store preserve the actual admitted inputs.

The supplied CI session's VLM plan completes at tick 12. With 32 completed
cycles, cells and voltage reach tick 32, colonies reach tick 16 and the paused
then resumed environment reaches tick 30. Finite VLM completion is expected;
the harness does not silently stretch a plan or enlarge SIM3's supported horizon.

The host has one queued command and two admitted storage tasks including the
active task. A controlled gate holds the storage worker for the declared interval.
The second storage task commits a marker and its exact receipt. Excess storage
and command admission must return capacity, while cached status remains readable.
The marker and receipt are verified after release. This is deliberately induced
**queue pressure**, not a measurement of a stalled physical device.

A separate 1 MiB quota store rejects an over-budget ordinary commit without
changing its root/generation, then accepts a valid small commit and passes audit.
That check establishes quota admission and recovery; it does not simulate ENOSPC,
filesystem failure or storage-controller behavior. Its small files are retained
separately from the reported main session disk inventory.

At each configured checkpoint the host reaches an absolute cycle target and
becomes idle. A historical observation, payload and receipt are read and verified
through the same serialized storage lane used by continuous adapters. The runner
samples process resources and inventories the quiescent store, then admits the
next target. No unbounded task stream or per-observation measurement array is
created.

At the declared restart cycle, the host drains, stops and closes its store. A new
`NativeSession` and host reopen it. The canonical store identity, root, generation,
completed cycle, absence of a pending cycle, and all five engine state digests
must match exactly before execution resumes. Final model clocks and full audit
must pass. This is an **orderly in-process host/store restart**, distinct from
native subprocess termination, OS crash or power loss.

## Profiles and duration gates

| Profile | Cycles | Wall pacing | Restart after cycle | Minimum active workload duration | Maximum total duration |
|---|---:|---:|---:|---:|---:|
| `mixed-ci.cevqualification` | 32 | 2 ms | 16 | No minimum | 3 minutes |
| `mixed-24h.cevqualification` | 8,760 | 10 seconds | 4,380 | 24 hours | 36 hours |
| `mixed-7d.cevqualification` | 5,160 | 2 minutes | 2,580 | 7 days | 9 days |

The long profiles are runnable operating experiments, **not achieved release
qualification**. Select and retain a profile identifying the actual declared
device and budgets before running it. The checked-in storage label explicitly
says that a dedicated device declaration is required. Do not turn a failed run
into passing evidence by editing its recorded budgets or report.

Long profiles use wall pacing to remain inside the finite model horizon. Their
planned cycle pacing includes 20 minutes and four hours of headroom, respectively,
above the minimum duration; the measured minimum gate remains authoritative.
The active workload timer includes actual host operation, controls, historical
probes, sampled inventories and the declared orderly restart. Creation of the initial canonical definition, the separate quota probe and final
audit are included only in total wall time. Compatible session open/engine
admission at each host launch is inside the active workload interval.
The minimum timer is checked against measured monotonic elapsed time: no
accelerated logical clock, injected sleep after completion, or CI smoke run
can satisfy a 24-hour or seven-day result. Pacing starts from completion of each
cycle; filesystem work may lengthen the run. These profiles cover a paced host
with finite VLM completion and one restart, not arbitrary source churn or
continuous mixed-peer delivery.

## Report interpretation

The JSON schema is `anaiero.mixed-qualification/1`. A successful report requires
all correctness checks and every declared budget. A failed correctness check or
budget produces a nonzero exit status and an inspectable report when the process
can publish one. An externally killed process may leave an incomplete directory;
that is not a passing run.

The report includes bounded logarithmic histograms for three separate operations:

- Cached snapshot call latency, sampled during progress and blocked storage.
  This excludes the intentional event wait and is not remote IPC latency.
- Durable engine-control completion latency, including queue/commit completion.
  The CI workload has three samples: pause, exact pause retry and resume.
- Historical probe completion latency, including storage-lane admission, indexed
  observation lookup and complete payload/receipt digest verification. Index-node
  work is recorded independently. These probes run at idle cycle boundaries,
  so the result is not a loaded-peer fairness measurement.

Percentile fields are conservative fixed-histogram **upper bounds**, not exact
interpolated percentiles. The report declares
`percentile_method=log2-ceiling-capped-by-observed-maximum/1`: the selected
nearest-rank bucket ceiling is capped by the exact observed maximum, using the
same fixed 65 counters. See the [estimator definition](README.md#percentile-estimator)
for integer boundary behavior and treatment of historical reports. Exact sample
counts, minimum and maximum remain in the report. Current/peak process memory,
CPU and available context-switch counters,
owner wakeups, completed storage tasks, queue high-water samples and rejections
are also recorded. Queue high-water fields describe sampled cached state rather
than instrumentation of every queue transition.

The disk measurement covers sampled regular-file logical/allocation bytes and
file count in the main store, excluding directories, journal and filesystem
metadata. The process high-water RSS includes harness and model admission work.
Reopen timing includes native recovery and compatible engine admission; page-cache
state is uncontrolled. This report does not measure network ACKs, OS/device
power-loss durability, a package upgrade, biological validity or independent
scientific calibration.

`anaiero_mixed_qualification` runs the short profile in the existing native CTest
matrix on Windows and Linux. It verifies exact retained input hashes and model
clocks, every declared correctness check, non-destructive rerun rejection,
invalid profile rejection and preserved negative-budget reports.
