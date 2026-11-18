# Native operating profiles

These profiles qualify a **synthetic canonical evidence and lifecycle workload**.
They measure the actual native store, indexed reads, recovery and lifecycle
services. They do not run the five-engine application mix or authenticate a
remote peer; those paths have separate native integration tests. Passing this
profile does not establish a 24-hour daemon soak or a seven-day release gate.

The executable has no Python or external numerical runtime dependency. Build
and run from the repository root:

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --parallel 3
cmake "-DANAIERO_BUILD_DIR=build/native" "-DANAIERO_PROFILE=qualification/native/evidence-100k.cevqualification" "-DANAIERO_OUTPUT=build/evidence-100k" -P native/cmake/RunNativeQualification.cmake
```

`anaiero qualify run PROFILE NEW_OUTPUT_DIRECTORY` is the full distribution
entry point. `ce_native_qualification` provides the same operation in the
controller distribution. The output directory must not exist. A repeated run
uses another directory; it never replaces earlier evidence or a user's store.
The directory retains the exact profile bytes, the generated canonical store
and `report.json`. A failed budget or correctness check returns a nonzero exit
status and records the failure in the report when the native process remains
able to publish it. An externally killed run is incomplete evidence.
Quote the complete `-DNAME=value` argument when invoking the CMake launcher,
including in PowerShell. The launcher reports its resolved executable, profile
and output paths before starting the workload.

## Predeclared profiles

| Profile | Throughput observations | Closed source epochs | Historical checkpoints | Accounted store limit | Regular-file disk limit | Process peak RSS limit |
|---|---:|---:|---|---:|---:|---:|
| `ci-smoke.cevqualification` | 256 | 4 | 128, 256 | 128 MiB | 256 MiB | 256 MiB |
| `evidence-100k.cevqualification` | 100,000 | 100 | 10,000, 100,000 | 2 GiB | 4 GiB | 512 MiB |
| `evidence-1m.cevqualification` | 1,000,000 | 100 | 10,000, 100,000, 1,000,000 | 16 GiB | 32 GiB | 512 MiB |

The file contains the complete numeric gates, including commit/lookup p99,
reopen time, maximum files and index-node visits. The parser requires every
budget, rejects duplicate/unknown fields and bounds the workload before any
store is created. Select budgets and the storage label before the run. Do not
edit a failed report or relax a threshold after observing a result and present
it as the same qualification.

The million-observation profile is an available dedicated-runner experiment,
not an assertion that this scale has passed. Its disk budget includes the
allocation cost of many small files; allocated bytes can substantially exceed
serialized accounting. Ensure the selected device has capacity for the actual
profile, operating-system metadata and temporary work.

## Work and measurements

Each throughput observation has a source/epoch/sequence identity and a unique
512-byte synthetic payload. One canonical transaction commits up to 63
observations, its receipt object and a guarded cursor record. No result is
counted before commitment. The epoch workload separately registers, activates,
attaches, advances, drains, seals and retires one source at a time. It contributes
one additional observation per epoch. A 100,000/100 report therefore covers
100,000 throughput observations and 100 lifecycle observations, while keeping
at most one source active. Historical epoch reuse and ownership replacement
must still be rejected after active capacity is recycled.

At each history checkpoint the runner closes and reopens the store **before**
performing deterministic old/middle/recent point queries. It verifies unchanged
store/root/generation/cursor identity, counts index-node visits, checks a missing
sequence and inventories the store. A final reopen verifies that retired
sources remain absent from active state. Full canonical audit is timed and its
visited records/nodes are reported separately.

The JSON report includes:

- Exact profile SHA-256 and compiled source/build identity; native OS/kernel,
  compiler, architecture, filesystem identifier and logical processor count.
- Batch-commit latency and point-query latency using a fixed 65-bucket base-2
  histogram. The p50/p95/p99 fields are **upper bounds**, not exact percentile
  interpolations. Minimum and maximum samples are exact monotonic durations.
- Ingestion time for preparing/committing throughput batches. Total wall time
  additionally includes queries, reopens, inventories, lifecycle churn and
  full audit. The batch-commit histogram excludes lifecycle administration.
- Healthy-reopen latency and node visits; per-query maximum node visits;
  canonical audit counts and time. Page-cache state is not controlled, so
  healthy reopen is not a claim about cold-device recovery latency.
- Actual derived-index reads/writes, staged nodes, transient writes elided,
  spill count and peak bounded cache size during throughput commits. These
  expose write amplification directly; removing temporary path-copy versions
  does not weaken canonical blob/transaction/root flush ordering.
- Native current/peak resident or working-set bytes and process CPU time.
  Linux context-switch counters are exposed; Windows marks those counters
  unavailable instead of inventing comparable values. Peak memory is the
  operating system's process high-water mark, including the harness.
- Canonical accounted bytes, logical regular-file bytes, allocated regular-file
  bytes and file count at each checkpoint and after lifecycle churn. The
  allocation inventory excludes directory, journal and filesystem metadata.
  It is a sampled store inventory, not an instrumented peak during every write.

Allocation counts, device IOPS, CPU wakeups, host dispatch/projection latency
and remote-ACK latency distributions are not measured by this executable.
The continuous host and mesh expose bounded wake/page/byte/queue counters;
their functional tests establish ordering and progress within their fixtures.
They do not substitute for those missing deployment measurements.

## CI and release evidence

`native-modular.yml` runs the smoke profile as a native CTest and runs the
100,000-observation/100-epoch profile separately on Windows and Linux. It retains
the report, exact profile, build manifest and native test logs. The package gate
installs the distribution and rejects modified files, additional files and a
different declared builtin identity. `native-mesh.yml` separately requires real
native TLS and local IPC for authenticated shared-session integration.

The [2026-11-18 shared-sandbox preflight](../reports/2026-11-18-shared-sandbox-100k/README.md)
preserves a completed failed result: correctness, memory, disk and indexed-read
gates passed, but the batch-commit p99 upper bound exceeded its unchanged
2-second budget. It is pre-publication operating evidence, separate from the
hosted reports for an exact reviewed commit.

Qualification records must identify the exact commit/run and distinguish:

| Evidence level | What it establishes |
|---|---|
| Deterministic injected cuts | Recovery at named software publication boundaries |
| Actual process termination | Recovery after a native process exits or is killed; the OS remains running |
| Orderly close/reopen profile | Indexed startup, retained identity and measured workload costs |
| OS-crash experiment | Only established by an explicitly documented crashed-host run |
| Device power-loss experiment | Only established on the declared filesystem/controller/device setup |

The latter two are not inferred from `fsync`, write-through flags, orderly
reopen or process-kill tests.

The remaining M6 release gate requires pinned hardware/filesystem/dependencies,
a 24-hour mixed integration soak, a seven-day declared daemon profile, measured
fairness/tail-latency under churn and lag, actual storage-pressure experiments,
and supported installation/upgrade/uninstallation evidence. Reports from
ephemeral CI runners provide functional and comparative operating evidence;
they do not qualify an unspecified production deployment.
