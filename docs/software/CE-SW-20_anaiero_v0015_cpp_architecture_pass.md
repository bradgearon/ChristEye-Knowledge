---
title: "CE-SW-20 — Anaiero v0.015 Native C++ Architecture Pass"
project: "ChristEye Anaiero"
version: "0.015.0"
status: "Proposed target architecture and audited migration plan; not an implemented unified daemon"
classification: "Digital simulation and read-only scientific observation software"
reviewed_commit: "5f5ab33a6668b712c54dc15327a568e77af67a98"
date: "2026-11-16"
related: "CE-SW-01, CE-SW-03, CE-SW-11, CE-SW-12, CE-SW-18, CE-SW-19, CE-A09, CE-A17, CE-A22"
---

# Anaiero v0.015: native architecture review and target design

## 1. Decision summary

Anaiero will be a **modular C++20 controller runtime** with one public command, explicit application use cases, small passive models, deterministic simulation engines, typed observation/movement contracts, and infrastructure adapters. **Linux and Windows are equal first-class, lightweight native controller deployments.** They share one portable C++20 runtime core, contracts, deterministic engines, and bounded resource policies; distinct infrastructure adapters implement each OS's event loop, IPC, persistence, identity checks, and service lifecycle. No WSL or container is needed. Components retain freestanding-compatible, bounded cores where useful.

**No big-bang rewrite.** Preserve the tested SIM1/VLM1/VLM2/SIM3/SW4/SW5 functionality and wire formats while introducing a single composition root, canonical observation model, and transactional outbox. The first target runs **simulated cells and digital models**. Qualified instruments may later produce **read-only observations** via separately authenticated, reviewed adapters. It is neither a biological culture controller nor evidence of real-cell learning.

The public **anaiero** command is currently a dispatcher to ce_* programs. A future **anaiero runtime start** mode must own an actual long-running supervisor; a command wrapper alone does not constitute integrated orchestration.

## 2. Audit: what exists at the reviewed commit

| Area | Current implementation | Remaining boundary |
|---|---|---|
| Wire codec | WindPacket/1, 56-byte header, bounded binary encode/decode, TLVs | No authenticated transport/session |
| Scheduling | WindBus bounded weighted scheduler; SW4 urgency-weighted 96-item queue | SW4 selection performs bounded quadratic scanning, no per-peer quotas |
| Digital simulation | SIM1, VLM1, VLM2, SIM3 cultures, SW5 Cell Behavior Atlas | SIM3 and SW5 use distinct cell-state engines |
| Behaviors | Versioned synthetic cell parameters, development/lineage/candidate links | Not experimentally calibrated; domain events are not transactional with all sinks |
| Runtime state | Single-writer SW4 ColonyProjector, coherent mutex-protected copies | Projector, epochs and producer cursors not restored from durable journal |
| Persistence | SHA-256 objects, committed HEAD, cursors, snapshots | Separate culture files and message sink lack one atomic outbox; store capacity 4 MiB / 4096 events |
| Learning | CEVPLAN, digital Crystal/RNA references, model fidelity and selection | No validated neuronal memory or genome-to-behavior mapping |
| Viewer | SVG/OBJ, NDJSON and interactive projected scene | Viewer is not a live authenticated subscription endpoint |
| CLI/build | anaiero routes to native backends; CMake and Windows/Linux tests | Monolithic header-only build, no service composition API or runtime packages |
| Media | Digital PCM/MIDI patchbay with bounded port/route checks | No actual operating-system I/O or media mesh |
| Security | Local source allowlists and serialized contracts | No crypto identity, onboarding, trusted enrollment, mTLS or peer ACL |

**Source-backed architecture debts** identified in this review:

1. native/include/christeye/living/colony_runner.hpp owns SIM3 state, simulated food/data transfers, persistence and rendering in a single header. Its resource movement bypasses the new movement port.
2. native/include/anaiero/services/runtime_service.hpp includes infrastructure/weighted_queue.hpp, and infrastructure/windbus_adapter.hpp includes services/runtime_service.hpp. Infrastructure/cell_checkpoint.hpp also includes a concrete cell behavior service. These are existing layer inversions, not new architecture.
3. native/include/anaiero/infrastructure/cell_bus_adapter.hpp receives a post-mutation callback, and counts bus failures after the domain state was changed. A crash or full queue can leave simulation and projected state at different revisions.
4. native/include/anaiero/infrastructure/sim3_ingress.hpp maintains transient source sequence numbers and resets them in each new adapter. Epoch and durable cursor ownership are not formalized.
5. native/include/anaiero/infrastructure/windstore_sink.hpp publishes each metric frame using the event kind CULTURE_CHECKPOINT, which conflates individual observations with full culture checkpoints, and syncs files on a dispatch path.
6. native/include/anaiero/infrastructure/movement_router.hpp maps distinct digital transfer kinds into a generic connection metric. Movement semantics should remain typed end to end.
7. The SW4 queue scans the bounded slot array against itself for source ordering; latency is deterministic in bound but not an acceptable scaling design without target benchmarks.
8. The controller has no trusted real acquisition adapter, native Linux/Windows IPC service, durable projector rebuild or authenticated mesh. GitHub CI success does not qualify these absent features.

## 3. Bounded-context map

Anaiero has **several models with different purposes**, not one mythical universal cell object:

- **Simulator context**: SIM1/VLM1/VLM2 instructions and deterministic synthetic fields; SIM3 Colony/Cell snapshots; SW5 CellBehaviorService and World. Engines remain independently versioned.
- **Observable context**: canonical, source-qualified digital observation envelopes and current ColonyState/Animon state. This is the **interoperability language**, not a replacement for every simulation-specific class.
- **Learning context**: CEVPLAN staged programs, synthetic needs/desires, training, model selection and evidence review. Needs never authorize physical support changes.
- **Artifact/evidence context**: immutable snapshots, raw acquisition references, schemas, provenance, cursors and indexed events.
- **Control-plane context**: host configuration, source identity, capability scopes, subscriptions, load limits, health and peer lifecycle.
- **View context**: time-indexed read models, read-only visualization, SVG/OBJ/NDJSON, user subscriptions.

Never merge SIM3 and SW5 cell structs by field name or equal numeric cell ID. Use fully scoped **CultureId + EngineId + EntityId + Epoch** identities and explicit projections. Candidate links, observed structural connections and demonstrated functional links are different evidence types.

## 4. Target component and dependency diagram

~~~text
                     anaiero (public CLI / composition root)
                                   |
                         anaiero runtime start
                                   |
           +---------------- Application / use cases ---------------+
           | RunTick | CommitCulture | Replay | IngestObservation   |
           | Subscribe | Evaluate | JoinPeer | ProposeLearning      |
           +---------------------+----------------------------------+
                                 |
                 +---------------+----------------+
                 |                                |
          Pure domain services               Port contracts
          +--------------------+            +---------------------+
          | CellBehaviorEngine |            | IClock              |
          | ColonyProjector    |            | IEventIngress       |
          | AnimonPriority     |            | IOutbox             |
          | LearningPolicy     |            | IEventJournal       |
          | EvidenceValidator  |            | IStatePublisher     |
          +--------------------+            | ISimulationMovement |
                    |                      | ISourceIdentity      |
              Passive models               | IObservationSource   |
                                           +----------+----------+
                                                      |
                                     +----------------+--------------+
                                     |       Infrastructure          |
                                     | WindPacket, bounded queues    |
                                     | WindStore, host I/O adapters  |
                                     | Unix sockets, TLS/mesh        |
                                     | Sensor ingress (read-only)    |
                                     +-------------------------------+
~~~

Dependency rule: **models -> nothing; contracts -> models; services -> contracts/models; application -> services/contracts; infrastructure -> contracts/models; host/composition -> everything required**. A service may receive an interface implemented in infrastructure but should not import a concrete infrastructure header. Compatibility adapters wrap legacy christeye::living modules, while new public headers use anaiero namespaces. Do not rename persisted wire identifiers merely to rename C++ namespaces.

**Proposed CMake target graph:**

~~~text
anaiero_models                 (passive types)
anaiero_contracts              (typed messages / ports)
anaiero_domain                 (deterministic policies, no OS APIs)
anaiero_application            (transaction / runtime coordination)
anaiero_simulators             (SIM1, VLM, SIM3, cell-atlas adapters)
anaiero_infra_wire             (WindPacket codecs, digital patchbay)
anaiero_infra_storage          (WindStore, durable I/O, checkpoint adapters)
anaiero_infra_host             (portable host interfaces)
anaiero_infra_linux            (epoll, UDS, systemd)
anaiero_infra_windows          (IOCP, named pipes, Windows services)
anaiero_infra_secure_mesh      (later: authenticated peer transport)
anaiero                       (CLI + composition root)
anaiero_testkit                (synthetic fixtures / fault injection)
~~~

Use CMake INTERFACE libraries for truly pure/header-only components initially; move oversized host implementations to compiled STATIC libraries as interfaces stabilize. Keep a small build without networking, viewers, or physical-source adapters. Keep legacy ce_* programs available during migration, then make them internal build options rather than uninstallable hidden dependencies.

## 5. Canonical events and observation envelope

All **cross-module movement** must occur through typed ports, with versioned schema and no direct cross-service mutation. Proposed stable envelope fields:

~~~text
ObservationEnvelope/1:
  event_id, schema_id, payload_type, payload_version
  culture_id, engine_id, subject_id, source_id, source_epoch
  source_sequence, source_clock_domain, acquired_time, time_uncertainty
  observed_or_simulated_tick, measured_unit, validity, confidence_class
  origin = SIMULATED | IMPORTED_UNVERIFIED | QUALIFIED_READONLY
  evidence_object_sha256, profile_sha256, calibration_sha256
  causation_id, correlation_id, permission_scope
~~~

The **source epoch** is mandatory for restarts, replacements and device reconnects. A sequence is monotonically increasing *within* (source, epoch), not globally. Authenticated principal identity is established by the ingress session, never by a self-reported source field. Origin and quality remain separate: a simulated value can be current and coherent but not biologically measured.

Type families include:
- CellSnapshot, CellLineageDelta, CellMaturityProxy, CandidateConnection, MeasuredConnection, LearningProxy, TrainingEvidence
- CultureSupportObservation, AcquisitionHealth, AnimonDigitalDesire, AnimonReviewNeed, PeerStatus
- DigitalMovement, MeasurementRef, CheckpointCommitted, PlanProposed, DataGap, ReplayCompleted.

Do not label each individual metric as a full CULTURE_CHECKPOINT. Publish a checkpoint event once per immutable snapshot; publish observations with their own kinds. Preserve all source-specific units, sampling limitations and uncertainty; a SHA-256 is **byte integrity**, not an author signature, material identity or scientific validation.

## 6. Exactly where state and evidence commit

There are **two independently useful products**: immutable evidence and volatile current state. Specify their ordering:

1. A simulation engine runs one deterministic tick against a staged NextState and collects a bounded EventBatch with canonical source identity.
2. The application commits **NextState + outbox event references together** under a single recoverable transaction identity. The current separate .state atomic rename and WindStore publication do not yet form this transaction; an outbox store or transaction coordinator must be built.
3. After durable commit, a bounded dispatch worker loads the outbox, emits typed bus events, and acknowledges each event by immutable event ID. At-least-once delivery plus idempotent projection is expected.
4. The projector validates the source/epoch/sequence, applies a coherent revision, and pushes a read-only snapshot/delta to subscribers. The journal records enough information to reconstruct every materialized value.
5. On restart, verify committed snapshot tip, replay outbox and durable observation journal after the checkpoint revision, restore per-source cursors, reconcile duplicates, mark sources OFFLINE_CACHED/STALE until re-established, then expose the new epoch.
6. Backpressure never silently discards a committed lineage or evidence event. Optional high-rate telemetry can be coalesced with an explicit DataGap receipt and loss count; urgent review is independent of critical hardware safety.

The outbox is a digital **delivery contract**, not exactly-once physical effects. Network partitions preserve local state and stop unauthenticated or stale remote writes. Snapshot readers never see half-applied revisions. Filesystem fsync does not guarantee every possible power-loss mode on every device; qualify actual storage.

## 7. Scheduler and service behavior

Maintain separate **priority, urgency, fairness, and authorization** concepts. Digital Animon desires must not compete with or control essential physical care.

Proposed runtime dispatch architecture:

- Dedicated bounded source FIFOs preserve each source+epoch ordering; per-lane indexed heads avoid current O(Capacity²) scanning.
- Fault/status, health observations, digital desires, evidence and telemetry use independently bounded admission pools. Fault reports may preempt but must not starve unrelated work without a maximum residency policy.
- Smooth weighted round robin or deficit round robin applies across eligible lane heads. Urgency+age+deadline ranks only eligible work within a lane. Per-source credits and per-peer byte quotas control flooding.
- For capacity exhaustion: reject new requests with explicit receipt; retain committed outbox entries; coalesce only designated replaceable telemetry; never drop source sequence gaps invisibly.
- Budget fixed work units per wakeup, yield to the host event loop; expensive JSON, SHA, disk sync and large imaging conversions do not run on time-critical lanes.

Ownership: one writer per culture/projector, many immutable readers. Use an ordinary mutex and bounded copy while contention is low; optional double buffering / RCU only after profiling. Memory-mapped snapshots are for immutable views and do not replace transactional persistence.

## 8. Lightweight native host topology: Windows and Linux

**Equal platform requirement:** `anaiero runtime start` must work natively on **Windows and Linux**, with the same C++20 domain/application core, binary wire format, source-epoch semantics, bounded queues, culture recovery, projection, and public CLI. Both are complete supported controller targets. Neither needs WSL, Docker, Cygwin, Python/Node, or an embedded web server for headless operation. OS-specific headers belong only in infrastructure adapters selected by the composition root.

| Host concern | Linux | Windows |
|---|---|---|
| Event-loop wake / timers | `epoll`, `eventfd`, `timerfd` | IOCP, overlapped I/O, events, waitable timers |
| Local IPC | Unix-domain sockets | Named pipes with overlapped I/O |
| Local identity | `SO_PEERCRED` plus authorization policy | Pipe ACL, access tokens, impersonation validation |
| Process lifetime | systemd, signal handling | Service Control Manager (SCM), service control events |
| Storage lock | `flock` | `LockFileEx` |
| Durable files | `fsync`, rename, supported directory sync | `FlushFileBuffers`, write-through replacement |
| Memory maps | `mmap` | `CreateFileMappingW`, `MapViewOfFile` |
| Clock | `CLOCK_MONOTONIC` | `QueryPerformanceCounter` or appropriate monotonic timer |
| Diagnostics | bounded structured logs/journald | bounded structured logs/Windows Event Log |
| Packaging | native ELF package | native PE/EXE (MSVC) package |

A Windows named-pipe client ID is not authentication: verify its security token and effective scope. A Unix socket path is not authorization: verify peer credentials and effective scope. The transport-independent source/epoch identity contract remains authoritative.

**Topology:** one low-overhead headless supervisor process with an event/IPC owner, a single domain mutation owner, and a bounded asynchronous persistence worker. Remote mesh, viewers, imaging and scientific importers are optional adapters, not compulsory resident dependencies.

**Memory and CPU budgets:** provisional starting goal ~64 MiB *supervisor* memory, excluding simulation heaps and bulk sensor/media assets; this is not a measured guarantee. On Linux report RSS/PSS; on Windows report Private Bytes and Working Set. Benchmark allocations, CPU wakeups, queue residency, p50/p95/p99 latency, replay and abrupt-exit recovery on representative Windows and Linux controller hardware. No busy polling, no unbounded buffers and no disk flush on latency-critical threads. OS process overhead differs, but functional limits and bounded policies must be equivalent.

**Release gate:** a new controller feature is incomplete until both native host adapters (or an explicitly approved portable implementation) pass shared admission, restart, state freshness, epoch, security scope, backpressure, and IPC tests. Windows CI uses MSVC; Linux CI uses GCC/Clang. Hardware benchmarks are separate from CI and must be reported per OS. The existing Windows/Linux C++ test suite is not proof that the future Windows/Linux service hosts are already built.

Choose the controller OS based on deployment needs, never on a Linux-first architectural assumption. See [ADR-002](ADR-002_native_windows_linux_parity.md).

## 9. Simulation versus qualified observation

The same **read model and transport contracts** can display both categories, but not assign the same provenance or infer cell identity.

| Role | Digital simulator | Reviewed physical readout |
|---|---|---|
| Engine | SIM1/VLM/SIM3/SW5 symbolic rules | Instrument/OEM read-only acquisition adapter |
| Meaning | Dimensionless assumptions | Qualified measurement with units, clock and uncertainty |
| Source identity | Synthetic producer + run epoch | Authenticated device + study/preparation/assay |
| Cell lineage | Digital generation and tombstone | Observed candidate identity with evidence uncertainty |
| Learning | Digital counter and task evaluation | Independently assessed experimental endpoint |
| Permitted effect | Mutate digital objects and synthetic routes | Report measurements and review notifications only |

No WindBus, Animon desire or experimental planner may issue real feeding, cell differentiation, molecular administration, electrical stimulation or biological culture changes. Physical essential care remains the responsibility of an independently qualified facility-owned system, regardless of the simulator or runtime condition.

## 10. Security and mesh plan

Today the application has source allowlists, but **no native authenticated join**. The secure mesh requires:

1. Provisioned node identity and rotation/revocation, authenticated enrollment, explicit preparation-aware scopes.
2. TLS 1.3 / authenticated secure transport backed by a maintained library; do not implement homegrown cryptography.
3. A handshake exchanging signed node identity, protocol version, capability and schema IDs, clock quality and nonce/challenge; deny permission until binding completed.
4. Explicit read-vs-write permissions and per-peer observation filters; least privilege, no remote access to physical actuator paths.
5. Durable resume negotiation using epoch+cursor+tip hash and authorized object manifests; reject state divergence, gap and replay attempts.
6. Rate/byte limits, quotas, path sanitization and telemetry of denied joins. No arbitrary dynamic plugin code from network input.

A device's valid WindPacket or SHA-256 hash does not grant a trusted identity.

## 11. Testing strategy and proof obligations

**Architecture**: automated include-layer rules and an explicit baseline of existing dependency inversions; no new model-to-infrastructure or contract-to-service dependencies. Eventually reduce the waiver list to zero.

**Core**: property-based bounded parsing, deterministic snapshot and process replay, malformed inputs, capacity limits, no hidden heap in hot paths.

**Integration**: SIM3 and SW5 -> canonical events -> durable outbox -> WindBus -> projector -> subscriber -> restart/replay, including injected failure at each commit boundary. Source epochs, duplicated events, gaps and idempotent recovery must be tested.

**Performance**: max-load multi-source priority/fairness, p99 on target board, bounded memory, subscriber slowness, no starvation. Instrument raw acquisition must not overwhelm control traffic.

**Security**: denial of unauthenticated peers, role/scoping, replay, revoked sessions, malformed packets and local Unix socket credential checks before any data reaches projection.

**Scientific**: recorded observation provenance, calibrated units and uncertainty, independent held-out datasets, source-specific comparisons. A test passing on synthetic data does not prove biological realism.

## 12. Migration without breaking working code

| Proposed release | Small, mergeable milestone | Exit gate |
|---|---|---|
| 0.015.x | Architecture decisions, source map, dependency guard | Documentation and test baseline green |
| 0.016 | Canonical envelope, source epoch and transactional digital outbox | Recovery/duplicate/gap tests across kill points |
| 0.017 | Application coordinator and SIM3/SW5 adapters behind common ports | Golden outputs unchanged; no post-commit lost events |
| 0.018 | Equal native Linux/Windows supervisor, host IPC and durable projector rebuild | Cross-platform conformance and per-OS controller benchmarks |
| 0.019 | Secure mesh identities, scopes and authenticated replication | Negative security and partition tests |
| 0.020 | Read-only qualified acquisition and calibration intake | Data provenance, uncertainty and independent review gates |

Do not promote an implementation maturity version merely because architectural milestones are numbered SW1-SW5. **Anaiero remains v0.015 experimental** until later criteria are met.

## 13. Prioritized refactor order

**P0**: Correct observable consistency: outbox/atomic commit, source epoch, replaying projector, proper event types, no post-mutation silent bus failure.  
**P0**: Define composition root, canonical IDs and contracts; maintain a single writer per culture.  
**P1**: Untangle includes and CMake targets, migrate SIM3/SW5 through adapters, retain binary/schema compatibility.  
**P1**: Windows/Linux native event-loop IPC and bounded bulk-data path; queue scalability and backpressure tests.  
**P1**: Signed trust, authenticated mesh and scoped subscriptions before remote access.  
**P2**: Scientific observation importers, parameter calibration, visualization integration and richer simulation laws only after provenance/uncertainty gates.

**Acceptance for this architecture pass**: maintain this document and ADR, introduce executable dependency-layer regression tests, link from the software index, and create an implementation backlog. This pass is a **design/guardrail release**, not a claim that the daemon, outbox, or mesh is built.
