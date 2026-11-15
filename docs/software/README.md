# ChristEye Anaiero — Native Software Architecture

**Status:** Proposed C++20 architecture and research-software design; not an implemented production transport, verified biological controller, or validated physical Crystal system.

## Specifications

| Contract | Purpose | Implementation status |
|---|---|---|
| [CE-SW-01: Native Mesh, WindBus and Evidence Changesets](CE-SW-01_native_mesh_architecture.md) | Cross-platform C++20 core; compact versioned WindPacket; authenticated sessions; interrupt/event-driven transport; weighted endpoint queues; append-only content-addressed WindStore; durable WindChange DAG and provenance; secure replication | **Design only** |
| [CE-SW-02: Developmental WindBus and Evidence-Guided Integration](CE-SW-02_developmental_windbus_architecture.md) | Read-only developmental observation and evidence records for candidate cell lineage, axons, neurites/spines, local RNA measurements, multirate sensor fusion, developmental hypotheses, experimental case histories, and validated digital-model improvements | **Design only** |
| [CE-SW-03: Colony Intelligence, Bread/Meat Planner and Support Backplane](CE-SW-03_colony_intelligence_bread_meat_backplane.md) | Live materialized colony state, revisioned push deltas, digital Bread/Meat compilation, source-linked team guidance, read-only serial diagnostics, Ethernet/USB3/fiber board connections and secure plug-in onboarding | **Design only** |
| [CE-SW-04: Twelve-Axis Control Surface, Crystal Evidence and Teacher Vision](CE-SW-04_control_surface_crystal_vision_teaching.md) | Twelve software preference axes per client/colony, host bounds, switchable teacher/digital CrystalPack strategies, Crystal presence/evidence sessions, voice/text input/output, and camera data/feature routing over USB3/GigE | **Design only** |

See also [proposed Colony Intelligence and control schemas](../../schemas/software/README.md) for snapshot, board capability, Bread/Meat planning and guidance records.

## System integration

These documents extend the existing [v3 architecture](../../specifications/v3/master_architecture.md) without replacing its logical WindEnvelope contract, current Python prototype or laboratory safety boundary.

- **CE-A11** owns typed route contracts and permitted electronic connections; CE-SW-01 specifies future native/distributed implementation.
- **CE-A19** owns authorized ingress and source quarantine; binary packets cannot bypass its authority.
- **CE-A20** owns immutable content and snapshots; WindStore details the future durable native implementation.
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

## Proposed implementation order

1. **SW1 — Core codec and simulator.** Shared freestanding WindPacket serializer/parser, fixed-size bounded queues, deterministic simulated endpoints, fuzz/property tests and version negotiation.
2. **SW2 — Native local durability.** WindStore journal, atomic content-addressed objects, deduplication, crash recovery, and receipt semantics.
3. **SW3 — Secure joins and peer replication.** Attested identities, route capabilities, platform transport adapters, reconnection reconciliation, loss/backpressure tests.
4. **SW4 — Developmental read-only observations.** Synthetic cell, imaging, electrophysiology and sparse molecular events; evidence-scored connection graph and developmental twin.
5. **SW5 — Independent evaluation.** Withheld-case comparisons, calibration of prediction uncertainty, reproducibility, offline replay and research-review gates.

**Release gate:** These are architectural specifications. Passing existing Python unit tests does not establish any of the SW1–SW5 milestones. No direct biological stimulation, culture changes, virus handling, or material-delivery commands are authorized by these proposals.

## Related documents

- [P02 WindBus connective-controller design](../../phases/P02/docs/design/P02-02_WindBus_Messages_and_Connective_Controllers_Design.md)
- [P02B neural acquisition and timebase design](../../phases/P02B/docs/design/P02B-09_Neural_Acquisition_and_Multimodal_Time_Base_Design.md)
- [CE-A11 connective controller design](../../specifications/v3/design/CE-A11_connective_controllers_Design.md)
- [CE-A22 independent limits design](../../specifications/v3/design/CE-A22_independent_limits_and_governance_Design.md)
- [Scientific source-provenance and publication policy](../../PUBLICATION_POLICY.md)
