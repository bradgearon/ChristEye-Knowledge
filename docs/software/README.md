# ChristEye Anaiero — Native Software Architecture and Implementation

**Status:** implemented native C++20 research runtime on Windows and Linux,
with durable mixed-engine sessions, reusable digital experiments, supervised
scientific providers and optional authenticated evidence replication.
Production-scale qualification and empirical biological validation remain
separate milestones.

## Current capability map

Start with the [implementation checkpoint](native_implementation_status.md)
for what runs and the [readiness assessment and development plan](native_development_plan.md)
for ratings, priorities and completion gates.

### Engine names

| Historical code | Descriptive name | Meaning |
|---|---|---|
| SW5 | Cell Behavior Engine | Individual synthetic cells, state, candidate connections and input responses |
| SIM3 | Multi-Colony Engine | Synthetic colonies, resources, inter-colony links and scheduled processes |
| CEVLM/2 + CEVPLAN/1 | Virtual Machine and Plan Engine | Existing symbolic machine/process interpreter and staged programs |
| Scientific provider | Numerical Model Engine | Pinned numerical models such as passive voltage and scalar concentration |

These are different model/execution families. Historical codes remain aliases
in existing formats. VLM plans execute both standalone and through the compiled shared-session
binding in CE-SW-38. The complete explanation and architecture are in
[CE-SW-37](CE-SW-37_modular_runtime_and_evidence_lifecycle.md).

### Implemented integrations

| Integrated capability | Current specification |
|---|---|
| Reduced index work, mixed-host profiles, reviewed receiver retention and canonical package selection/rollback | [CE-SW-40](CE-SW-40_operating_envelope_and_package_lifecycle.md) |
| Exact timing, scale/recovery campaigns, leased bootstrap and verified cross-build rollback | [CE-SW-41](CE-SW-41_runtime_lifecycle_and_qualification.md) |
| Continuous canonical host/mesh, reviewed compaction and native package/profile tooling | [CE-SW-39](CE-SW-39_continuous_native_operation.md) |
| Compiled SDK, canonical evidence, real shared VLM and lifecycle/retention | [CE-SW-38](CE-SW-38_compiled_runtime_and_lifecycle.md) |
| Mixed native sessions, restart, operations and local control | [CE-SW-31](CE-SW-31_integrated_native_sessions.md) |
| Reusable SW5/SIM3 trials, durable receipts and SW5 outcomes | [CE-SW-32](CE-SW-32_reusable_native_experiments.md) |
| Segmented storage, checkpoint recovery and maintenance | [CE-SW-33](CE-SW-33_sustained_storage.md) |
| Owned native scientific processes and pinned continuation | [CE-SW-34](CE-SW-34_supervised_scientific_sessions.md) |
| Enrolled mutual TLS, scoped evidence replay and artifacts | [CE-SW-35](CE-SW-35_authenticated_native_mesh.md) |
| Scalar environmental model and frozen calibration evidence | [CE-SW-36](CE-SW-36_environment_and_calibration.md) |

### Native build profiles

| Profile | Operator entry point | Scope |
|---|---|---|
| Full native build | `anaiero` | Dispatcher plus compatible `ce_*` backends and legacy/demo tools |
| `ANAIERO_CONTROLLER_ONLY=ON` | `ce_modular_session`, `ce_modular_host`, `ce_native_qualification`, compatible legacy/scientific tools | Native session controller and selected utilities; does not build the `anaiero` dispatcher or discover Python |
| Either profile with `ANAIERO_ENABLE_MESH=ON` | `ce_modular_mesh`, host `--mesh`; legacy `ce_mesh`; full dispatcher `mesh-live` / `mesh` | Optional OpenSSL 3 canonical/legacy evidence transport |

Use the [native build instructions](../../native/README.md),
[mixed-session commands](CE-SW-31_integrated_native_sessions.md),
[scientific examples](../../examples/scientific/README.md) and
[canonical mesh guide](../../native/docs/continuous-mesh.md). A successful build does not
install an OS service; see the [deployment examples](../../deployment/native/README.md).

## Target architecture

[CE-SW-37 — Modular runtime, durable execution and evidence lifecycle](CE-SW-37_modular_runtime_and_evidence_lifecycle.md)
is the target architecture. [CE-SW-38](CE-SW-38_compiled_runtime_and_lifecycle.md)
implements its M1–M4 batches in the modular session. The architecture specifies
compiled public/private modules, registered capabilities, recoverable state/
evidence transactions, verified indexes, complete lifecycle/retention rules,
continuous mesh scheduling and native qualification gates. Its M1–M6 sequence
is reflected in the development plan. [CE-SW-39](CE-SW-39_continuous_native_operation.md)
implements the continuous canonical path, reviewed compaction and M6's native
package/profile tooling. [CE-SW-40](CE-SW-40_operating_envelope_and_package_lifecycle.md)
adds receiver retention, conservative package activation/rollback and actual
mixed-host qualification. Sustained release qualification remains a distinct gate.

## Historical specifications and increment records

The status column below records each document's original increment. Later
CE-SW-31–40 integrations supersede several original “pending” statements.
Use the current capability map above for present behavior; preserve these
records for design history and the scientific boundaries of each increment.

| Contract | Purpose | Status when introduced |
|---|---|---|
| [CE-SW-01: Native Mesh, WindBus and Evidence Changesets](CE-SW-01_native_mesh_architecture.md) | Cross-platform C++20 core; compact versioned WindPacket; authenticated sessions; interrupt/event-driven transport; weighted endpoint queues; append-only content-addressed WindStore; durable WindChange DAG and provenance; secure replication | **Design only** |
| [CE-SW-02: Developmental WindBus and Evidence-Guided Integration](CE-SW-02_developmental_windbus_architecture.md) | Read-only developmental observation and evidence records for candidate cell lineage, axons, neurites/spines, local RNA measurements, multirate sensor fusion, developmental hypotheses, experimental case histories, and validated digital-model improvements | **Design only** |
| [CE-SW-03: Colony Intelligence, Bread/Meat Planner and Support Backplane](CE-SW-03_colony_intelligence_bread_meat_backplane.md) | Live materialized colony state, revisioned push deltas, digital Bread/Meat compilation, source-linked team guidance, read-only serial diagnostics, Ethernet/USB3/fiber board connections and secure plug-in onboarding | **Design only** |
| [CE-SW-04: Twelve-Axis Control Surface, Crystal Evidence and Teacher Vision](CE-SW-04_control_surface_crystal_vision_teaching.md) | Twelve software preference axes per client/colony, host bounds, switchable teacher/digital CrystalPack strategies, Crystal presence/evidence sessions, voice/text input/output, and camera data/feature routing over USB3/GigE | **Design only** |
| [CE-SW-05: Modular Audio/Vision/MIDI StarChip and Patchbay](CE-SW-05_modular_audio_vision_midi_starchip.md) | Replaceable power-off analog AFE/op-amp cards, multichannel 24-bit PCM, MIDI 2.0 UMP/MIDI-CI, live teacher voice/speech, secure typed media routing, three board models and verified per-port capabilities | **Design + digital-only admission reference** |
| [CE-SW-06: SIM1 Living-System Instruction Simulator](CE-SW-06_living_surrogate_instruction_simulator.md) | Bounded executable C++20 symbolic crest/blood/neuron/glia model, 32-byte instruction codec, moving synthetic participants, molecular proxy registry, event replay and WindPacket integration tests | **Software surrogate implemented; biological fidelity unvalidated** |
| [CE-SW-07: Virtual Living Machines, Portable Setup Files and Replay](CE-SW-07_virtual_living_machine_bundles.md) | Docker-like host-side VLM1 images, single-file machine bundles, instruction journals, conservative 8×8 Soil proxy movement, clone/fork/verify/stream CLI and deterministic tests | **Digital-only machine implemented; biology uncalibrated** |
| [CE-SW-08: Process-linked VLM2 Timelines and Visual Assets](CE-SW-08_process_timelines_and_visual_assets.md) | Extended single-file CEVLM/2 process catalog, duration/weight/movement, synthetic process image refs, timed action scheduling, legacy VLM1 upgrade, NDJSON render frames, and WindPacket integration tests | **Native deterministic surrogate implemented; not biological validation** |
| [CE-SW-09: CEVPLAN/1 Learning and Crystal/RNA Evidence Manifests](CE-SW-09_learning_crystal_rna_plan_manifests.md) | SHA-256-pinned stage manifests, reverse machine mappings, local plan runner and synthetic receipts | **Digital-only reference + native tests** |
| [CE-SW-10: Learning and Crystal/RNA Evidence Registry](CE-SW-10_windstore_learning_crystal_registry.md) | Host-only SHA-256 immutable objects, append-only event journal, stage-to-hash lookup, filtered push inbox, imported plans and non-mutating successor proposals | **Digital-only reference + native tests** |
| [CE-SW-11: Durable WindStore and Evidence Cursors](CE-SW-11_durable_windstore_journal_and_cursors.md) | OS-level exclusive writer lock, committed HEAD, crash-tail recovery, synchronous evidence push, disk audit and durable subscriber cursors | **SW2.1 host-only implementation; cross-platform validation pending** |
| [CE-SW-14: SIM3.2 Model Fidelity and Calibration Evidence](CE-SW-14_model_fidelity_calibration.md) | Hash-pinned synthetic law profiles and frozen observations, training/holdout evaluation, deterministic error/uncertainty reports and WindStore receipts | **Native read-only evaluator with cross-platform tests; biological calibration not demonstrated** |
| [CE-SW-15: SIM3.3 Frozen Model Selection and Final Test](CE-SW-15_model_selection_independent_test.md) | Candidate law profiles ranked by training-only error, locked winner, reserved final-test SHA, strict held-out evaluation and synthetic-only results | **Executable training-only selector and integration tests** |
| [CE-SW-17: Anaiero unified native command](CE-SW-17_anaiero_unified_native_cli.md) | Public C++20 `anaiero` CLI for virtual machines, plans, WindStore, colonies, lab, fidelity, and model selection | **Implemented + cross-platform smoke tests** |
| [CE-SW-19: Cell Behavior Atlas and Developmental Surrogate](CE-SW-19_cell_behavior_atlas_and_developmental_surrogate.md) | SHA-256-pinned nine-cell-type behavioral records, source provenance, synthetic development/training/connection rules, checkpoints and SW4 WindBus projection | **Native digital surrogate + integration tests; biological fidelity unvalidated** |
| [CE-SW-18: Bounded ColonyState, Animon priorities and controller runtime](CE-SW-18_colony_state_runtime_projector.md) | Thread-safe latest state, weighted event-driven queues, typed WindPacket movement, simulated-only priority and optional WindStore adapter | **Native prototype and tests; no physical actuation / secure mesh** |
| [CE-SW-13: SIM3.1 Unified Living Laboratory](CE-SW-13_unified_living_laboratory.md) | Versioned synthetic laws, SHA-pinned VLM2 projections, WindStore checkpoint indexes, synthetic reference evaluations, validated frame schemas and interactive 3D replay | **Native + viewer prototype; biology not validated** |
| [CE-SW-12: SIM3 Persistent Multi-Colony Runner and 3D Models](CE-SW-12_persistent_multicolony_runner.md) | Restartable SHA-256-pinned host runner, two-colony synthetic resource/data/Stimubus links, lineage tombstones, read-only mmap checkpoint views, golden SVG/OBJ test artifacts and source-provenance descriptors | **Host-side digital-only prototype and native tests** |
| [CE-SW-20: Anaiero v0.015 Native C++ Architecture Pass](CE-SW-20_anaiero_v0015_cpp_architecture_pass.md) | Audited dependency map, controller topology, event/outbox ordering, scheduling, security and safe migration roadmap | **Architecture proposal with regression guard; no new daemon or mesh** |
| [ADR-001: Native Runtime Ownership](ADR-001_native_runtime_ownership.md) | Target ownership boundaries, common observation envelope, source epochs and transactional digital outbox | **Proposed decision; implementation pending** |
| [CE-SW-22: SW5 Forked Optimization and Durable Routes](CE-SW-22_forked_simulation_optimization_and_durable_routes.md) | Actual SW5 forks, multi-goal scoring, CAS-pinned route state and recoverable outbox | **Native synthetic reference; authenticated delivery pending** |
| [CE-SW-24: Scientific Simulation Foundation](CE-SW-24_scientific_simulation_foundation.md) | C++20 multirate SW5 neuron–astrocyte hypothesis comparison, optional NEURON/BRAINCELL/PhysiCell/MOOSE/STEPS offline traces, WindBus/ColonyState integration | **Native reference implemented; optional scientific solvers not bundled or validated** |
| [CE-SW-25: Source Epochs and Multi-Source ColonyState Recovery](CE-SW-25_source_epoch_recovery.md) | Epoch-aware WindPacket schema, durable replay of authorized observation sources, source freshness and restart checks | **Native local prototype; remote trust/epochs pending** |
| [CE-SW-30: Engine-Neutral Optimization and Verified Session Gates](CE-SW-30_generic_engine_optimizer_and_trust_gates.md) | Actual SW5 fork adapters behind reusable scenario ports, matched-horizon scoring, durable digital commits, explicit remote session authorization policy | **Native digital-only strategies; actual encrypted mesh still pending** |
| [CE-SW-29: Async Evidence and Exact Unit Contracts](CE-SW-29_async_evidence_and_units.md) | Bounded storage worker, commit-before-projection, checkpoint index and non-destructive retention review with exact unit conversions | **Native reference, not production-journal compaction** |
| [CE-SW-28: Native Windows/Linux Host and Weighted Queue](CE-SW-28_native_controller_host_and_queue.md) | Same-user local IPC server, console-hosted digital SIM3 control, source FIFO and linear weighted scheduling | **Native executable; SCM/systemd registration, secure mesh and async persistence pending** |
| [CE-SW-27: Reusable Simulation Engine Kernel](CE-SW-27_reusable_simulation_kernel.md) | Injectable engine lifecycle, SW5/SIM3 adapters, bounded coordinator and checkpoint contracts | **Native digital-only reference; atomic engine-group transactions pending** |
| [CE-SW-26: Live Multirate Scientific Solver Coupling](CE-SW-26_live_multirate_solver_coupling.md) | Bounded live step/reply with optional NEURON sidecar, SW5 development, hypothesis testing and epoch-aware WindBus recording | **Native + optional external model; not a validated biological simulation** |
| [CE-SW-23: Durable Route Receipt Bus and Projector Replay](CE-SW-23_durable_route_bus_projection.md) | Typed route WindPacket receipts, content-addressed journal, local projector ACK, idempotent restart replay and subscriptions | **Digital-only local integration; secure mesh and complete epochs pending** |
| [CE-SW-21: Adaptive Optimization and Service Extensions](CE-SW-21_adaptive_optimization_and_service_extensions.md) | Goal-directed digital route proposals, bounded dry runs, safe revision-checked commits and postcommit extension hooks | **Native synthetic-only reference; durable changes pending** |
| [ADR-002: First-Class Native Windows and Linux](ADR-002_native_windows_linux_parity.md) | Lightweight portable C++20 controller with equal native Windows/Linux host adapters and release gates | **Accepted platform requirement; native supervisor adapters pending** |

See also [proposed Colony Intelligence and control schemas](../../schemas/software/README.md) for snapshot, board capability, Bread/Meat planning and guidance records.


## System integration

These documents extend the existing [v3 architecture](../../specifications/v3/master_architecture.md) without replacing its logical WindEnvelope contract, current Python prototype or laboratory safety boundary.

- **CE-A11** owns typed route contracts and permitted electronic connections; CE-SW-01 records the original distributed design and CE-SW-35 implements the bounded native evidence mesh.
- **CE-A19** owns authorized ingress and source quarantine; binary packets cannot bypass its authority.
- **CE-A20** owns immutable content and snapshots; CE-SW-33 records the current sustained native storage integration.
- **CE-A12** coordinates plans; **CE-A22** independently authorizes or vetoes any qualified physical action.
- **CE-A16** owns retained Root-evidence contracts, not inferred cell identity or unverified memory.
- **P02/P02B** describe instrumentation limits, independently timestamped acquisition, and the simulated observation-only bridge.
- **CE-SW-02** adds the developmental observation plane; it does not introduce an autonomous biological intervention service.

## Architectural invariants

1. No application-level polling for new messages: event-triggered push, OS readiness/completion, callbacks, hardware interrupts and DMA notifications.
2. All message types share negotiated binary encoding, authenticated identities, versioned schema, causal lineage and explicit receipts; raw sensor streams may use separate data lanes.
3. Durable delivery uses local journal commits, explicit acknowledgements, at-least-once forwarding and idempotent digital application; do not claim exactly-once physical effects.
4. Scientific **confidence** is separate from queue **priority**, measurement quality, and authorization.
5. Sensors and biological observations retain real acquisition time, hardware clock domain, calibration, transform digest, and uncertainties. Sparse molecular sampling remains sparse.
6. New cells are observation candidates until evidence supports lineage and integration claims; axons and dendritic spines are neuronal structures, not separately cloned cell identities.
7. Growth/connection/Crystal hypotheses remain hypotheses until independent experiments validate them; a network or protocol is not evidence that a biological learning mechanism exists.
8. Unauthorized or stale messages cannot override laboratory equipment, A22 limits, safety interlocks or regulator-approved study controls.

## Original implementation sequence

This SW1–SW5 outline records the initial architecture sequence. The
[current development order](native_development_plan.md#revised-delivery-order)
starts from the integrated implementation and governs new work.

1. **SW1 — Core codec and simulator.** Shared freestanding WindPacket serializer/parser, fixed-size bounded queues, deterministic simulated endpoints, fuzz/property tests and version negotiation.
2. **SW2 — Native local durability.** WindStore journal, atomic content-addressed objects, deduplication, crash recovery, and receipt semantics.
3. **SW3 — Secure joins and peer replication.** Attested identities, route capabilities, platform transport adapters, reconnection reconciliation, loss/backpressure tests.
4. **SW4 — Developmental read-only observations.** Synthetic cell, imaging, electrophysiology and sparse molecular events; evidence-scored connection graph and developmental twin.
5. **SW5 — Independent evaluation.** Withheld-case comparisons, calibration of prediction uncertainty, reproducibility, offline replay and research-review gates.

**Release evidence:** assess each implemented capability using its native
integration and failure tests, not Python tests alone. Passing software tests
does not establish biological validity or authorize physical intervention.
The current assessment links the exact native Windows/Linux results and
identifies the operational qualification still needed.

## Related documents

- [P02 WindBus connective-controller design](../../phases/P02/docs/design/P02-02_WindBus_Messages_and_Connective_Controllers_Design.md)
- [P02B neural acquisition and timebase design](../../phases/P02B/docs/design/P02B-09_Neural_Acquisition_and_Multimodal_Time_Base_Design.md)
- [CE-A11 connective controller design](../../specifications/v3/design/CE-A11_connective_controllers_Design.md)
- [CE-A22 independent limits design](../../specifications/v3/design/CE-A22_independent_limits_and_governance_Design.md)
- [Scientific source-provenance and publication policy](../../PUBLICATION_POLICY.md)

## Current development

- [Native implementation checkpoint](native_implementation_status.md)
- [Readiness assessment, priorities and acceptance gates](native_development_plan.md)
