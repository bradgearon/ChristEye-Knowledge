# Native operating profiles

The `evidence-*` and `ci-smoke` profiles qualify a **synthetic canonical evidence
and lifecycle workload**. They measure the actual native store, indexed reads,
recovery and lifecycle services. They do not run the five-engine application mix
or authenticate a remote peer. The separate [mixed host profiles](MIXED.md) run
all five compiled engine families through the continuous host. The
[scale/lifecycle profiles](SCALE.md) combine throughput, returning and newly
introduced source identities, receiver retention and canonical compaction; authenticated
transport has its own native integration tests. Passing a short profile does not
establish a 24-hour daemon soak or a seven-day release gate.

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

The new `evidence-10k-exact`, `evidence-100k-exact` and `evidence-1m-exact`
profiles use `ANAIEROQUALIFICATION/2` and explicitly declare
`latency_gate=exact-nearest-rank-threshold/1`. Their numeric latency limits are
unchanged. Original version 1 profiles remain byte-for-byte unchanged and retain
the original conservative histogram gate. Every new report identifies its
selected gate and includes both exact threshold counts and the legacy bound.

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
- Exact counts above each inclusive latency limit, plus batch-commit latency
  and point-query latency using a fixed 65-bucket base-2
  histogram. The p50/p95/p99 fields are **upper bounds**, not exact percentile
  interpolations. The selected bucket ceiling is capped by the exact observed
  maximum; minimum and maximum samples are exact monotonic durations.
- Disjoint native commit-stage timings for lock waiting, validation, staging,
  blob persistence, transaction persistence, canonical ROOT publication, derived
  index publication and staging cleanup. Each fixed-memory histogram also
  records a saturating duration sum with an explicit saturation flag. Timing is
  opt-in on the native store and leaves canonical bytes/publication order intact.
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
  regular-file budget keeps its original scope. Directory counts and allocated
  directory bytes are reported separately on Linux; Windows explicitly marks
  directory allocation unavailable. Allocation-unit size, volume-wide free
  bytes and free file-node counts where available expose additional pressure.
  Filesystem journals and inode-table costs are not attributed to the store.
  Inventories are sampled rather than instrumented peaks during every write.

Process allocator counts and device IOPS remain unmeasured. Native commit-stage
elapsed time identifies the expensive stage but does not by itself distinguish
CPU work, filesystem delay and host scheduling. The mixed path separately
measures host dispatch, queue wait, durable step commit and projection. Real TLS
ACK timing belongs to the mesh disturbance campaign; synthetic throughput never
claims a remote transport measurement.

### Percentile estimator

Synthetic and mixed reports declare
`percentile_method=log2-ceiling-capped-by-observed-maximum/1`. For a nonempty
histogram with `n` samples, percentile `p` selects the bucket containing sorted
rank `ceil(n*p/100)`. Its bound is the smaller of that bucket's inclusive
power-of-two ceiling and the exact maximum sample. The final bucket saturates
at `UINT64_MAX`; empty and all-zero histograms return zero. Both operands are
upper bounds on the selected observed quantile, so taking their minimum
preserves that guarantee. The histogram still stores exactly 65 counters and
the exact count/extrema, with no per-sample history.

For example, a selected bucket ending at 2.147483648 seconds with an observed
maximum of 1.6564229 seconds returns at most 1.6564229 seconds. A true observed
p99 above the declared 2-second budget still fails. All profile thresholds and
workloads remain unchanged. Earlier reports without this method identifier
retain their original bucket-only estimates and original pass/fail decisions.
They are not rewritten or retroactively reclassified; the corrected estimator
requires fresh execution with its exact source/build identity recorded.

## Exact threshold decisions and device declarations

For `n` measured durations, an inclusive percentile limit at percentile `p`
passes exactly when the number strictly above the limit is no greater than
`n - ceil(n*p/100)`. At p99 this is `floor(n/100)`. A 100-sample run may have one
sample above the limit; a 99-sample run may have none. Equality with the limit
passes. Empty samples cannot qualify. This requires three fixed-width counters,
not retained per-sample timing history. Independent sorted-sample oracle tests
cover all percentiles, rounding boundaries, inclusive equality and overflow-safe
rank calculation. The displayed logarithmic quantile remains an upper bound.

Version 2 profiles require an exact retained `ANAIERODEVICE/1` declaration before
output creation. It declares name, shared/dedicated class, machine, processor,
host-visible physical memory, storage device/firmware/controller, filesystem,
mount options, durability policy, dependencies and concurrent work. All fields
are required; duplicates and unknown fields reject. A declaration is operator
input and is not independent attestation of exclusivity or firmware.

The CMake capture helper records visible OS/CPU/memory information and marks
unavailable properties explicitly. Shared-runner defaults are diagnostic.
Dedicated declarations require explicit values for every deployment-specific
field. CMake reports physical memory in MiB; the helper records bytes at that
resolution ([CMake documentation](https://cmake.org/cmake/help/v3.20/command/cmake_host_system_information.html)).

```text
cmake -DANAIERO_DEVICE_OUTPUT=build/device.cevdevice -DANAIERO_DEVICE_NAME=shared-preflight -P native/cmake/CaptureNativeDevice.cmake
cmake "-DANAIERO_BUILD_DIR=build/native" "-DANAIERO_PROFILE=qualification/native/evidence-100k-exact.cevqualification" "-DANAIERO_DEVICE=build/device.cevdevice" "-DANAIERO_OUTPUT=build/exact-100k" -P native/cmake/RunNativeQualification.cmake
```

Capture or supply the declaration before execution. The command preserves its
original bytes and SHA256 beside the profile. New `anaiero.native-qualification/2`
reports include the exact decision and the historical upper-bound result side by
side, plus stage timing and allocation detail. Passing an exact threshold does
not rewrite a previously failed version 1 result. Selecting a different declared
method is separate from improving storage performance.

## CI and release evidence

`native-modular.yml` tests both profile versions and runs the new exact-gate
100,000-observation/100-epoch profile separately on Windows and Linux. It captures
a shared-runner device declaration before execution and retains its exact bytes
with the report, profile, build manifest and native test logs. This is a new
versioned acceptance method; historical results remain unchanged. The package gate
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

## Mixed host profiles

The separate [mixed native host guide](MIXED.md) covers `anaiero qualify mixed`
and `ce_native_qualification mixed`. That path runs all five compiled engine
families through the real continuous host, exercises bounded queue pressure and
quota rejection, verifies exact orderly restart, and preserves pinned inputs and
measured reports. The short CI profile is a functional operating check; its
24-hour/seven-day profiles enforce actual measured duration and remain unachieved
until complete reports are retained for a declared deployment.

## Provisioning for history and compaction

The synthetic runner checks for at least `4 × observations + 128` available
file nodes before opening its store: one distinct payload and at least three
index rows per observation. The [maintenance profile](SCALE.md) checks
`7 × observations + 128` because both the current index and its replacement
can coexist during compaction. These are necessary lower bounds in these
workloads, not sufficient volume sizes. Filesystem free bytes, configured
store and work limits, directory allocation, retained history and other users
of the volume still matter. Where the filesystem exposes no comparable free
file-node count the report says so; byte gates remain active. A failed
preflight retains inputs, the requirement, measured capacity and the reason;
no observation or latency sample is attributed to that attempt.

[Campaign operation](MIXED.md) covers durable create/resume/status, the
real TLS fixture, the isolated full-filesystem experiment and the dedicated
elapsed-duration profiles. The [CE-SW-41 evidence record](../reports/2026-11-18-runtime-lifecycle/README.md)
distinguishes development diagnostics from qualification of a particular
checkout, machine or duration.

## Derived index comparison

Use the [paired layout experiment](PACKED-INDEX.md) for an explicit layout,
coexistence file counts, and matched Linux/Windows trials.
