---
title: "CE-SW-31 — Integrated Native Sessions, Recovery and Host Control"
status: "Native C++20 bounded digital runtime"
platforms: [Windows, Linux]
---

# CE-SW-31 — One recoverable native session

## Operating outcome

A configured native controller now composes SW5 and SIM3 through the common
simulation lifecycle, records observations asynchronously, projects them only
after durable receipts, runs real SW5 fork analysis, and applies/reverts a
selected digital route in the active session. Its local control endpoint reads
immutable cached state while a dedicated owner executes engine and disk work.

The public entry remains `anaiero runtime`. Existing `.cev` start/simulate/serve
commands remain supported. A `.cevsession` selects the integrated controller.

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# A bounded session without any IPC requirement; reopen the same directory.
./build/native/anaiero runtime session examples/runtime/mixed-native.cevsession ./mixed-state 4
./build/native/anaiero runtime session examples/runtime/mixed-native.cevsession ./mixed-state

# Linux local endpoint. On Windows use an endpoint name such as anaiero-mixed
# and build/native/Release/anaiero.exe.
./build/native/anaiero runtime start /tmp/anaiero-mixed.sock examples/runtime/mixed-native.cevsession ./mixed-state
./build/native/anaiero runtime query /tmp/anaiero-mixed.sock STATE
./build/native/anaiero runtime query /tmp/anaiero-mixed.sock "STEP 4"
./build/native/anaiero runtime query /tmp/anaiero-mixed.sock "ANALYZE 101 3 4 18"
./build/native/anaiero runtime query /tmp/anaiero-mixed.sock APPLY
./build/native/anaiero runtime query /tmp/anaiero-mixed.sock "REVERT 101"
./build/native/anaiero runtime query /tmp/anaiero-mixed.sock STOP
```

Commands that change the session return `QUEUED <ticket>`. This acknowledges
bounded command admission, not durable execution. Query `STATE` for
`last_ticket`, `last_result`, `session.busy`, `session.error`, proposal and
route status. Accepted step execution persists its target cycle count before
the next engine advances. A process restart resumes that recorded work.

## Configuration

`CEVSESSION/1` is a bounded text format with at most eight engines:

```text
CEVSESSION/1
name mixed-reference
period_ms 100
engine 101 sw5 61 1 1 "../cells/crest-blood-neural.cevatlas" "two-neurons.cevcells" 75 0 64 1
engine 201 sim3 71 1 2 "../connected-gardens.cev" "-" 0 0 0 0
end
```

An engine row contains `id kind source epoch cadence model initial support
stress signal lesson`. Paths resolve relative to the session file. The
configuration digest includes normalized configuration bytes and the model
and initial-state contents, so editing an input cannot silently change an
existing session. IDs and sources must be unique, epochs positive, cadences
1–64, and periods 10–60,000 milliseconds. SW5 inputs are validated proxies;
SIM3 uses its own model inputs and requires zero generic input fields.

The initial state is paused. `RUN` enables periodic logical cycles; `PAUSE`
stops new timer-triggered cycles. The period is a desired host cadence, not a
real-time or physiological time guarantee. Slow work applies backpressure;
the controller does not race to catch up an unbounded timer backlog.

## Ownership and dependency boundaries

| Component | Responsibility |
|---|---|
| `NativeSessionService` | Session decisions, recovery sequencing, evidence batches, route analysis/application |
| `SimulationCoordinatorService` | Per-engine scheduling and exact partial-cycle continuation |
| `CheckpointingEngineService` | Checkpoint-before-publication and explicitly authorized committed-step replay |
| `AsyncObservationCommitService` | Submission-order projection after durable completion |
| `SessionCommandService` | Bounded command grammar and control-port dispatch |
| `NativeSessionApplication` | Composition of services and concrete adapters |
| `SessionHostWorker` | One serial domain owner and cached control views |
| `AsyncEpochJournalWorker` | One separately owned observation journal thread |
| `WindStoreRecordStore` | Immutable session manifests and revision checks |
| `NativeIpcServer`, `NativeHostLifecycle` | Local identity, framed IPC, deadlines, stop signals and service lifecycle |

The metadata/checkpoint Registry and observation Registry are separate. No
Registry is concurrently accessed by the session owner and storage worker.
Completion notifications wake the owner; worker callbacks never mutate
ColonyState. Projection still traverses the typed epoch observation adapter,
WindBus queue and observational Projector.

## Partial-cycle recovery protocol

1. `begin_cycle` resolves each due engine's inputs and captures a stable cycle
   identity. The host persists this intent before engine side effects.
2. The coordinator advances at most one due engine per `step_cycle`.
3. The engine wrapper stores its model-pinned checkpoint before allowing the
   resulting snapshot to be published. SIM3 also retains its own durable
   Runner transaction; arbitrary in-place SIM3 rewind remains unsupported.
4. The session saves a pending snapshot publication, then submits its five
   typed metrics to the asynchronous journal. One observation is in flight
   for this session publication coordinator; the reusable async service also
   supports a configured bounded multi-inflight window.
5. After each durable receipt, the owner projects that observation and saves
   publication progress. A completed batch advances the source sequence and
   records its exact published snapshot.
6. Only then does the coordinator advance its engine cursor. A cycle completes
   when every due engine has completed its step and publication.

On restart the host validates the configuration and checkpoint identities,
replays the observation journal, then restores the saved coordinator position.
If exactly the unfinished engine committed one step before the host saved its
new position, only that engine is authorized to return that already committed
result. Other engines, extra ticks, changed model/source/epoch/cadence, and
unmatched inputs are rejected.

Publication retry retains the exact snapshot. An already committed observation
is replayed/idempotently acknowledged using the same source, epoch and sequence.
A later engine's failure does not repeat earlier engine steps. A publication
failure does not skip later due engines. These are per-engine recovery
guarantees; they do not assert an atomic global cross-engine transaction.

The metadata and checkpoints are CAS objects under WindStore. Catalog revision
is the engine revision plus one, because a fresh SIM3 engine has revision zero.
Model-pinned same-revision checkpoint writes are idempotent only if identical.

## Optimization in the active session

`ANALYZE <engine> <from> <to> <horizon>` supports a configured SW5 engine and
uses the existing `EngineOptimizationService` and `Sw5ScenarioEvaluatorService`.
It executes three actual digital candidate forks at guidance 400/700/900,
alongside matched-horizon baselines, under the engine's configured inputs. The
reference policy scores connections, viable participants and learning proxy,
including cost/risk/uncertainty penalties. Horizon is bounded to 128 ticks.

The analysis runs on the session worker, leaving IPC state/control responsive.
`CANCEL` requests cancellation between candidate evaluations, drops queued
commands and disables automatic cadence; it cannot interrupt an individual
filesystem sync or a single bounded evaluator invocation. RUN/new commands
can subsequently resume operation. A no-improvement analysis is recorded with
`approved=false` rather than inventing a selected change.

`APPLY` verifies the live engine ID, epoch, revision and full state SHA against
the recorded proposal. The session-owned `IEngineChangePort` stores the target
sealed checkpoint, route metadata and change identity in one durable manifest
before replacing the active world. Crash recovery completes that intent.
This avoids treating a separate demo route ledger as the active engine owner.

`REVERT <engine>` applies the existing route undo policy to the **current**
world. It restores the previous guidance intent and preserves elapsed ticks,
cell development and other state. It does not rewind the simulation. Route
status and the changed state are delivered through the same durable evidence
pipeline. An active route must be reverted before another route is applied
to that engine in this reference policy.

The proposal, source digest, baseline/trial metrics, chosen option, score and
commit ID remain in immutable session-manifest history. General CEVPLAN-driven
experiment execution and a real SIM3 candidate evaluator remain subsequent
work; this batch integrates the already implemented SW5 optimizer.

## Local control and operating-system lifecycle

| Command | Behavior |
|---|---|
| `PING` | Immediate availability response |
| `STATE`, `STATUS`, `ENGINES`, `SNAPSHOT` | Complete cached session, engine and metric view |
| `WATCH <view_revision>` | Wait up to 500 ms for a changed view, then return a complete view |
| `STEP [1..1024]` | Queue a bounded logical-cycle request |
| `RUN`, `PAUSE` | Enable/disable periodic new cycles |
| `ANALYZE`, `APPLY`, `REVERT` | Queue digital optimization operations |
| `CANCEL` | Cancel queued/ongoing analysis at supported boundaries and pause cadence |
| `RETRY` | Retry a retained engine-step failure |
| `STOP` | Reject new work, stop cadence, drain the active cycle/evidence, exit |

WATCH is a bounded complete-state watch, with a host-local view revision; a
reconnected client must accept the returned full snapshot and revision,
including a lower host view revision after restart. It is not a durable remote
subscription or an event-history cursor.

Linux uses same-UID Unix-domain sockets; Windows uses same-user named pipes.
One malformed, unauthorized, disconnected or stalled client is closed without
ending the server. Server and client I/O use absolute request deadlines. The
frame limits are 4 KiB requests and 16 KiB replies. Active endpoint ownership
is preserved; a second controller cannot silently replace a live endpoint.

The service owner and journal worker are independent of this bounded,
single-client-at-a-time control transport. An IPC request may wait behind
another request until its deadline; this is not a high-concurrency remote API.

`NativeHostLifecycle` implements console cancellation, POSIX SIGINT/SIGTERM,
systemd readiness/stopping notifications and Windows SCM status/stop handling.
See `deployment/native` for manually invoked deployment examples. SCM should
launch `ce_colony_runtime.exe service ...` directly; the public `anaiero`
dispatcher otherwise launches a child process and is not the SCM entrypoint.
No service is installed by building or testing this repository.

Graceful shutdown finishes the active cycle/change/publication. Already
persisted future target cycles remain recoverable on restart. A queued command
that has not executed is only an in-memory admission and can be canceled by
STOP. Accepted observation work is drained; completion slots are reserved so
notifications are not dropped. A hung OS filesystem operation still requires
external process supervision; no bounded fsync interruption is claimed.

## Capacity and explicit limits

The existing WindStore remains bounded to 4,096 events and a 4 MiB journal per
store. The session reserves conservative event capacity before beginning new
work, then reports `session_capacity_reached` at a recoverable boundary. It
does not claim indefinite unattended recording, segmented retention, pruning
or archival. Checkpoint and manifest persistence trades throughput for clear
crash boundaries in this reference implementation.

Scientific engines and physical units remain the CE-SW-24–30 optional model
integration paths. This session executes digital SW5/SIM3 models. Secure remote
mesh transport, physical control, empirical biological validation and actual
media devices are outside this implementation.

## Required verification

One local Linux measurement (GCC 14.2, Release, fresh example session) ran 50
logical cycles: SW5 tick 50, SIM3 tick 25, 385 committed observations and zero
sequence gaps. It took 6.985 s wall time and 1.000 s CPU time, with 9,472 KiB
peak RSS. The complete state occupied 1,147,679 bytes across 1,314 files.
This includes checkpoint/manifest creation and durable observation writes;
it is an environment-specific measurement, not a latency guarantee or Windows
qualification. It makes the current many-small-CAS-object storage cost visible.

Reproduce the workload with `anaiero runtime session
examples/runtime/mixed-native.cevsession <fresh-directory> 50` and a native
process resource monitor. The integration workflow sets `ANAIERO_REQUIRE_IPC=1`
so native CI cannot silently skip its socket/pipe qualification.

- Actual SW5+SIM3 mixed-cadence execution, byte-stable restart and independently
  decoded evidence sequence/count verification.
- Both partial-cycle retry regressions, preserved inputs, invalid recovery
  identity and one-step replay authorization.
- Separate process interruption immediately after durable evidence commit and
  after durable route intent, followed by recovery without duplication.
- Real SW5 analysis/application, stale proposal rejection and time-preserving
  revert; configuration/model changes rejected on reopen.
- Async completion ordering, full-queue shutdown, projection retries and
  explicit failed receipts.
- IPC malformed/stalled/disconnected clients, endpoint ownership, deadlines,
  cancellation, large frames and native lifecycle callbacks.
- Cached state while a driver is blocked, command capacity, cadence, WATCH,
  cancellation and shutdown with pending target cycles.
- Native Windows/Linux builds, existing regression suites and architecture
  guards. Socket-restricted execution environments report IPC unavailable
  (CTest return 77); ordinary native CI must actually execute the IPC test.
