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

The original JSON schema is `anaiero.mixed-qualification/1`; explicit `/2` profiles
produce `anaiero.mixed-qualification/2`. A successful report requires
all correctness checks and every declared budget. A failed correctness check or
budget produces a nonzero exit status and an inspectable report when the process
can publish one. An externally killed process may leave an incomplete directory;
that is not a passing run.

The report includes bounded logarithmic histograms for these separate operations:

- Cached snapshot call latency, sampled during progress and blocked storage.
  This excludes the intentional event wait and is not remote IPC latency.
- Durable engine-control completion latency, including queue/commit completion.
  The CI workload has three samples: pause, exact pause retry and resume.
- Historical probe completion latency, including storage-lane admission, indexed
  observation lookup and complete payload/receipt digest verification. Index-node
  work is recorded independently. These probes run at idle cycle boundaries,
  so the result is not a loaded-peer fairness measurement.

- Step dispatch: session intent lookup, engine preparation and queue submission.
- Step queue wait: queue submission through entry to the canonical writer.
- Step commit: actual `ICommitStore::commit` execution on the writer thread.
  The completion carries process-local monotonic timestamps; signal delivery and
  owner scheduling delay do not become storage latency.
- Step projection: adoption of the completed transition, including projection,
  inside the successful owner `SessionService::pump`. Synchronous control and
  cycle-boundary commits have separate scope.

The optional `host::ITimingObserver` receives bounded passive records on the
host owner. It must remain nonblocking and cannot reenter the runtime or store.
All four phase distributions have separate sample counts; none is an end-to-end
remote acknowledgement measurement.

Percentile fields are conservative fixed-histogram **upper bounds**, not exact
interpolated percentiles. The report declares
`percentile_method=log2-ceiling-capped-by-observed-maximum/1`: the selected
nearest-rank bucket ceiling is capped by the exact observed maximum, using the
same fixed 65 counters. See the [estimator definition](README.md#percentile-estimator)
for integer boundary behavior and treatment of historical reports. Exact sample
counts, minimum, maximum, cumulative nanoseconds and saturation state remain
in the report. `/1` profiles retain their original conservative-bound gates.
Explicit `/2` profiles add exact inclusive threshold counters for snapshot,
control, probe, dispatch, queue, commit and projection. Their predeclared
`exact-nearest-rank-threshold/1` decision accepts p99 only when the number above
the limit is at most `floor(samples/100)`; an empty distribution fails. The four
additional `maximum_*_p99_us` fields and `required_device_class=any|dedicated`
are required by `/2` and rejected by `/1`.
Historical reports and profiles retain their original decisions.

Current/peak process memory,
CPU and available context-switch counters,
owner wakeups, completed storage tasks, queue high-water samples and rejections
are also recorded. Queue high-water fields describe sampled cached state rather
than instrumentation of every queue transition.

The disk inventory includes regular-file logical/allocation bytes and file
count, plus directory counts/allocation and volume-wide free bytes/file nodes
where available. The `/1` disk gate retains its original regular-file basis.
The `/2` gate adds reported directory allocation. Neither measures inode tables,
filesystem journals, external metadata or auxiliary campaign report files. The process high-water RSS includes harness and model admission work.
Reopen timing includes native recovery and compatible engine admission; page-cache
state is uncontrolled. This report does not measure network ACKs, OS/device
power-loss durability, a package upgrade, biological validity or independent
scientific calibration.

`anaiero_mixed_qualification` runs the short profile in the existing native CTest
matrix on Windows and Linux. It verifies exact retained input hashes and model
clocks, every declared correctness check, non-destructive rerun rejection,
invalid profile rejection and preserved negative-budget reports.


## Durable campaign create, resume and status

The campaign path stores qualification progress in the same canonical store as
the mixed session. Its three native operations are:

```text
anaiero qualify campaign-create PROFILE SESSION NEW_DIRECTORY [DEVICE_DECLARATION]
anaiero qualify campaign-resume DIRECTORY
anaiero qualify campaign-status DIRECTORY
```

Use `ce_native_qualification` in place of `anaiero qualify` in the controller
package. Creation admits every input before creating the destination. The exact
profile, original session text, encoded definition including model/assets, and
optional `ANAIERODEVICE/1` declaration are retained. Elapsed profiles with a
positive `minimum_run_ms` require a device declaration. The shipped `/2`
24-hour and seven-day profiles require `device_class=dedicated`; a shared or
missing declaration rejects before directory creation. This is an operator
statement, including shared/dedicated class and known limitations; it is not
independent hardware or exclusive-device attestation.

Creation initializes the canonical session and campaign record, then returns.
Resume runs that exact admitted campaign. It compares the original profile,
encoded session, device declaration and compiled build identities before
admitting further effects. It loads embedded model/assets from the retained
session definition; changing or removing the original source asset paths does
not redirect the campaign. A different executable build starts a new campaign,
including when that executable is otherwise a compatible operational upgrade.

A separate native writer lease spans the entire invocation, including the
planned store close/reopen. Concurrent resume attempts fail `busy`. Every
absolute cycle target is committed before the host receives it. Checkpoints
follow completed work on the same serialized store lane. On recovery the actual
canonical completed/pending cycle is authoritative: progress beyond the last
checkpoint is accepted only within the durably admitted target. Stable pause,
resume and pressure operation identities prevent a second application of those
effects. A pending cycle finishes its original engine intents before further
cycles are admitted.

`campaign-status` reads an atomically published bounded cache while the campaign
holds its writer lease. The response says `cached:true` and `liveness:"unknown"`:
it is the last checkpoint, not a heartbeat or proof that a process is alive.
It contains the checkpoint root/generation, confirmed/target cycles and report.
Resume never authorizes work from this file. The canonical record is its source.

The state machine is bounded to 128 attempts and 65,536 checkpoints. Each
checkpoint report is limited to 128 KiB. Earlier attempts keep their last
canonical checkpoint report and acquire an explicit `interrupted` or `failed`
status before the next attempt. Final attempts also produce immutable
content-addressed report files. `report.json` is published only for a completed
workload. If the process dies after the final canonical checkpoint but before
file publication, resume republishes the exact retained report without running
another cycle or changing the canonical generation. Repeating a completed
campaign returns its original pass/fail result.

### Duration and measurement accounting after interruption

Each attempt uses a fresh monotonic clock. Only checkpointed active time is
added to `active_ns_lower_bound`. UTC timestamps identify attempts; wall time
between processes is never converted into active work. A failed or killed
attempt's uncheckpointed duration and latency samples are unknown.

The report exposes attempt count, interruption count, cycles reconciled beyond
the last checkpoint, and cumulative active-time lower bound. Its histogram and
exact-threshold coverage is **the current attempt only**. Earlier checkpoint
reports retain their own measured histograms and sums; resume does not combine
partially observed distributions or manufacture missing samples.

An interrupted campaign can finish and verify all 32 or 8,760 or 5,160 canonical
cycles. It then reports `campaign.completed:true` but
`campaign.continuous_qualified:false`, preserves the interruption, and returns a
failed qualification result. This is useful recovery evidence. It does not pass
a continuous-duration gate. A new uninterrupted campaign is required for that
claim. A final-report publication retry alone does not create an interruption
or alter its original result because the completed run was already canonical.

### Persistent-machine long runs

The explicit `/2` profiles are `mixed-campaign-ci.cevqualification`,
`mixed-campaign-24h.cevqualification`, and `mixed-campaign-7d.cevqualification`.
They retain the original cycle counts, pacing and minimum/maximum durations.
Their added dispatch, queue and commit p99 limits are five seconds; projection
is one second. These are new predeclared gates, not changes to the old profiles.

On the declared Windows or Linux machine, retain its device declaration and run:

```text
ce_native_qualification campaign-create qualification/native/mixed-campaign-24h.cevqualification examples/runtime/modular-native.cevsession run-24h device.cevdevice
ce_native_qualification campaign-resume run-24h
ce_native_qualification campaign-status run-24h
```

For the seven-day campaign select `mixed-campaign-7d.cevqualification` and a new
output directory. Use the persistent machine's normal supervisor or terminal
session with the working directory and executable pinned for the whole run.
The commands are native; Python is not required. A CI runner timeout, reboot or
service restart remains an interruption even when subsequent resume recovers
all accepted work. The supplied profiles and recovery tests are not completed
24-hour/seven-day evidence.

The campaign active interval covers its native host, controls, queue/quota
checks, probes, inventories, checkpoint publication and planned orderly reopen.
Final audit is included in invocation wall time. Each process has its own maximum
invocation-duration budget. The cumulative duration lower bound is diagnostic;
it does not replace the uninterrupted minimum or erase an earlier failed attempt.

## Five-engine TLS disturbance qualification

When the native mesh is enabled, `anaiero_mesh_mixed_campaign` runs the actual
five-engine sender through 32 cycles and real mutually authenticated TLS peers.
It exercises a withheld ACK alongside an independent receiver, receiver offline
growth and replay, actual certificate rotation, revocation, artifact retention,
canonical compaction and eight empty source registration/retirement epochs on
the live serialized lane. The finite VLM remains complete at tick 12.

This executable retains an exact workload declaration, original session text,
encoded admitted definition, executable digest and compiled build identity in
`mixed-mesh-tests/`. The report separates dispatch, queue, writer commit,
projection and remote ACK distributions. The remote observer spans offered-page
TLS write through canonical acceptance of its exact ACK, including receiver
work and sender acknowledgement persistence. Only successful exact ACKs enter
that distribution. Rotation/revocation rejection codes are reported separately.
The observer may run concurrently on client workers; it is borrowed, bounded,
thread safe and nonblocking. This test uses at most 1,024 samples per distribution
and an exact sorted nearest-rank p99 with a predeclared 60-second ACK limit.

This is short integrated host/mesh evidence. It does not establish sustained
fan-out, arbitrary source-cardinality reclamation, physical storage exhaustion,
OS/power-loss behavior or a full elapsed mesh soak. The eight churned sources
are explicitly empty lifecycle epochs; the five scheduled engines provide the
actual observations.

`anaiero_mixed_campaign` additionally cuts real subprocesses after a durable
engine commit, after a completed target before its campaign checkpoint, and
after the final canonical report before publication. It verifies original
model clocks/receipts, writer exclusion, changed-input rejection, immutable
attempt evidence, exact final-report retry and conservative continuity status.

## Actual filesystem exhaustion on an isolated volume

Build `anaiero_storage_pressure_tests` with native tests enabled. Its explicit
platform launcher provisions one new disposable 64 MiB filesystem and keeps the
result directory on a different volume:

```sh
bash qualification/native/run-isolated-pressure.sh /absolute/path/anaiero_storage_pressure_tests /absolute/path/new-pressure-report
```

```powershell
& .\qualification\native\run-isolated-pressure.ps1 -TestExecutable C:\build\Release\anaiero_storage_pressure_tests.exe -ReportDirectory C:\results\new-pressure-report
```

On Linux the launcher uses a new private `tmpfs` mount. On Windows it uses one
new fixed VHD, formatted NTFS and labelled `ANAIERO_PRESSURE`. The launchers need
permission to mount or attach that fixture; when unavailable they fail, rather
than approximating exhaustion by imposing an application quota. No script
selects a physical disk by number. The native executable requires an exact
ownership nonce, a distinct volume for the report, and a verified capacity of
8–64 MiB. An ordinary workspace is rejected before filler or store creation.

The first phase fills **only its exclusively created filler** to an actual
OS disk-full error. A subsequent ordinary commit must reject for insufficient
filesystem headroom without changing the canonical root or acquiring a receipt.
After releasing that filler, the second phase fills the same isolated filesystem
through a trusted test clock at the point **after** the store has checked free
space. It releases 64 KiB for the staging intent and attempts a 1 MiB blob write.
The store must report write failure, preserve its prior root and stop admission.
After freeing the filler, reopening must reconcile the exact intent and remove
the abandoned blob/temp before retry. The original committed receipt must still
verify, and the retried write must produce one durable effect and pass full audit.

`report.json` records the exact test build, volume capacity, actual filled
bytes, native disk-full codes, root preservation, reopen, exact retry and audit.
The matching profile is retained beside it. `anaiero_storage_pressure_guard` in
ordinary CTest verifies **only** that a workspace is refused; it cannot satisfy
the actual exhaustion gate. A successful tmpfs/virtual-disk run qualifies this
software fault path. Neither filesystem is evidence of physical disk failure,
controller durability, host crash, or power-loss safety.
