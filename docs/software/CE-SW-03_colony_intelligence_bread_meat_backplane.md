---
title: "CE-SW-03 — Bread/Meat Planning, Colony State and Participant Support Backplane"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
classification: "software systems design / simulated and read-only biological instrumentation"
status: "Proposed, not a commissioned board or validated biological control system"
related: "CE-SW-01, CE-SW-02, BIO-04, BIO-06, BIO-09, P02B-02, P02B-06, CE-A12, CE-A22"
---

# CE-SW-03 — Colony Intelligence and Participant Support Backplane

## 1. Objective and boundary

Give every colony an always-addressable, evidence-backed **current state** and a set of event-driven processes that compile introductory learning material (Bread), plan higher-order training (Meat), track support measurements (Milk/SustiBus, chemistry/ChemiBus), and publish guidance for qualified research staff. Make joining a new digital host, board or observation client a matter of connecting a standard supported interface, enrolling the device identity and selecting authorized readouts/subscriptions.

This is a SOFTWARE architecture. An inferred peptide-as-word, biologically stored semantic layer, physical Crystal delivering knowledge, or induced neuronal "yearning" is a research hypothesis, not an established implementation. No autonomous peptide synthesis, treatment formulation, molecular dosing, culture feed changes, neural stimulation, virus manipulation or tissue growth commands are issued by this runtime. Lab care, approvals, and independently qualified OEM systems remain separate.

## 2. One secure mesh; multiple purpose-specific applications

- **WindBus/Operations:** joining, routing, authenticated capability discovery, presence, telemetry, fault and service receipts.
- **WindBus/ColonyState:** state snapshots, revisioned deltas, member/region status, data-quality and staleness changes.
- **WindBus/BreadMeat:** immutable digital instructional artifacts, prerequisite graphs, skill-gap reviews, curricula, held-out evaluator reports.
- **SustiBus:** read-only care/service observations, source-qualified support status and human-approved service receipts.
- **ChemiBus:** observed molecular/chemical assay records and hypotheses; no commands containing sequences/doses/recipes.
- **WindBus/Development:** candidate cell lineage, neurite/axon/spine structural annotations, intermittent RNA measurements, integration evidence.
- **WindBus/Experiment:** investigator-approved study records, blinded outcomes and independently reviewed proposals.
- **WindBus/Acquisition:** bulk instrument data/chunk descriptors on bounded high-throughput links.

These are logically separated, permissioned routes under the CE-SW-01 native packet/security stack, NOT independent unauthenticated transports. Evidence priority is not evidence truth or physical action authority.

## 3. Bread Compiler — produce versioned digital learning primitives

Input: a target capability, authorized source materials, current Root/knowledge graph, prerequisite gaps, allowed task/codebook, accessibility and evaluator contract.

Pipeline: IMPORT/RIGHTS_CHECK -> DECOMPOSE_CONCEPTS -> CREATE_DEPENDENCY_GRAPH -> SELECT_PRIMITIVES -> BUILD_EXERCISES -> ATTACH_FROZEN_TESTS -> CANONICALIZE/HASH -> SIGN/REVIEW -> PUBLISH_DIGITAL_PACK.

Output **DigitalBreadPack** fields: pack_id, source_refs, concepts, layer_order, prerequisites, symbol/word codebooks, sample tasks, expected assessment boundaries, decoder version, license/permissions, builder digest, evaluator_split_ref, content hashes. A "layer" is a typed instructional abstraction (symbols -> relations -> concepts -> procedures) unless separately measured structural evidence establishes another meaning.

A phrase resembling a peptide "word" can be recorded as a **MolecularRoleHypothesis**: named literature-linked molecule family or candidate function, preparation/context, measured assay, receptor/receiver hypothesis, level F/X/H/D/A, confounders, uncertainty and independently approved study reference. It is NOT a digital word automatically convertible to an amino-acid sequence or an executable wet-lab recipe.

## 4. Meat Planner — choose next learning requirements

Input: Root test results, latest BreadPack dependencies, observed skill-gap graph, baseline/held-out performance, protected old skills, instrument readiness and evidence quality.

Output **DigitalMeatPlan**: candidate capability, learning frontier, prerequisite BreadPack IDs, multi-step digital tasks, planned train/evaluate/holdout splits, interference checks, software resource estimate, expected observations, stop/abstain criteria, evaluator owner. Plan status starts as PROPOSED and cannot bypass independent test-oracle separation.

A deficit in task performance is NOT evidence of nutritional deficiency, required cell growth or peptide intervention. The planner may propose an investigation of alternative explanations, including software decoder errors, timebase uncertainty or inadequate acquisition data. Only competent laboratory personnel and separate qualified SOPs can choose physical care changes.

## 5. ColonyState — always-addressable, locally durable materialized view

Each home host maintains an append-only event log (future WindStore) and publishes **atomic snapshots** and **monotonic revisioned deltas**. A subscribing client receives an authenticated snapshot plus the event stream starting after its snapshot revision; a reconnecting peer requests missing ranges by cursor on an event-driven handshake. It does not poll a database for new events.

State key: colony_id + preparation_id + epoch. The canonical event log owns truth; each materialized view is rebuildable. A colony home is the single authority for its current materialized state, while remote replicas can read and host independently signed interpretations. Concurrent scientific claims remain multi-value and evidence-linked, never last-writer-wins.

Snapshot top-level sections: identity/epoch/revision, lifecycle/readiness, preparation/regions, member_candidates, measured support (Milk), measured chemistry, acquisition health, neural/connectivity graph summaries, RNA assay references, functional Roots, digital learning plans, guidance queue, instrument/board roster, timebase quality, pending reviews and causal evidence references.

Each metric is a record with value/unit, sensor ID, acquired_time and clock domain, calibration/model digest, provenance, confidence or measurement uncertainty, missingness, freshness deadline, status VALID/STALE/UNKNOWN/DEGRADED/REJECTED. Missing readings cannot silently inherit a previous value. "Available" means the last authenticated local state is retrievable; **live** requires a fresh data source and healthy connection. Network outage never implies a healthy biological preparation.

## 6. Participants have different identity classes

- **DigitalPrincipal:** cryptographically authenticated board/host/agent identity with capability scopes, signed firmware/software digest and expiring credentials.
- **BiologicalMemberCandidate:** observation-only cell/region/colony annotation identified by imaging/assay evidence; no private key, network principal or execution permission.
- **InstrumentEndpoint:** vendor-qualified source reporting readouts and independent actual-delivery receipts.
- **ResearchActor:** approved human reviewer or evaluator, with role/approval provenance.

Biological joining is gradual evidence assessment, not an Ethernet join event. Neurites/axons and dendritic spines are neuronal structures, not separately enrolled/cloned cell identities.

## 7. Continuous observation pipeline

Device IRQ/DMA completion or vendor callback -> bounded capture descriptor -> host/MCU readout record -> WindPacket validation -> authenticated route -> acquisition journal -> calibration/time normalization -> typed observation -> colony projection update -> push revision delta -> incremental analytics -> evidence/needs proposal.

Run a *multirate* model: fast electrical/device events, intermediate image/structure events and sparse molecular endpoint assays. RNA sequencing is not a continuous live sensor. A predicted localization remains INFERRED and must cite its model/quality. Preserve hardware capture times separately from host arrival and analysis times; cross-modal alignment stores uncertainty.

## 8. Needs and research-advisory engine

Five independent output classes:
1. **Digital learning need:** unmet prerequisite, stagnant tested capability or protected old-skill regression.
2. **Evidence need:** absent calibration, stale sensors, weak cell tracking, conflicting conclusions or insufficient controls.
3. **Support review needed:** measured support deviation or OEM alarm; recommend human/SOP review, never autonomous feeding.
4. **Hardware/service need:** port failure, noisy serial bus, CRC errors, power/watchdog issue, disk/headroom or timebase fault.
5. **Scientific hypothesis:** an alternative explanation requiring registered study and independent assessment.

Need priority is an *operational scheduling heuristic* only: Score = AuthorizedUrgency × EvidenceQuality × ExpectedInformationValue / EstimatedComputeCost, with missing-data abstention and per-class quotas. Do not turn scores into calibrated biological probabilities without held-out calibration. Do not let untrusted publishers set their own class or weights.

The engine emits a GuidanceProposal with evidence refs, target colony/member, gap/goal, proposed nonphysical next observation or digital learning step, alternative explanations, uncertainty, protected interests, reviewer role, expiry and outcome. Physical care notes are explicitly REVIEW_ONLY and point to established lab SOPs without including a new delivery recipe.

## 9. Support motherboard / backplane — logical partitions

Start with the existing P02B board families rather than a new invasive headstage:
- A-B0 host: x86/ARM software host, storage, analytics, GPU and qualified vendor acquisition drivers.
- A-B1 supervisor MCU: read-only board diagnostics and external sensor status; watchdog, local queue and authenticated readouts.
- A-B2 independent limit hardware: local approved inhibit/fault state; out of band, cannot depend on WindBus availability.
- A-B3 qualified environmental OEM telemetry: service/alarm observations only.
- A-B4 chemistry sensor front-end: qualified measurement acquisition through MCU-supported interfaces.
- A-B5 image/sync trigger: vendor camera + timestamp/trigger adapters.
- A-B7 vendor MEA: preserve vendor-approved data/control path and grounding; no custom electrode drive.

A generic participant carrier may expose: one 1GbE Ethernet uplink (RJ45 with PHY, transformer and appropriate isolation), optional second management/backup uplink, USB service port, protected UART debug at board-level only, dedicated CAN-FD or isolated RS-485 instrument/serial fieldbus as supported, hardware timestamp/trigger input, bounded local nonvolatile event spool, board health sensors, secure-element option, and independent electrical power/fault circuitry.

Ports and pinouts remain candidates until board revision, EMI, isolation, cabling and OEM compatibility tests. Do not repurpose an Ethernet RJ45 port for raw TTL UART, CAN, or RS-485 wiring; avoid ambiguous connectors.

## 10. Data links: preferred choices

**Primary host/node mesh:** Standard Cat5e Ethernet at 1Gb/s for initial system management, state deltas and most digital evidence. For power, use standards-compliant PoE only if the device and power source are correctly rated and the safety assessment supports it; PoE/network uptime is not a substitute for independently qualified culture care.

**Direct instrumentation:** USB3 for supported cameras, microscopes and vendor peripherals, or GigE Vision for compatible network cameras. Attach cameras to the acquisition host and publish content hashes/metadata into WindBus. Do not flood every participant with raw video.

**Higher throughput / longer reach:** SFP+ 10GbE or faster fiber uplinks between hosts, switches/racks and storage; choose optics and switch hardware after bandwidth/cable/EMC characterization.

**Board-level monitoring:** MCU internal SPI/I2C for local sensors; protected board UART/USB CDC for bench readout; CAN-FD/RS-485 for suitable fieldbus peripherals with correctly specified isolation and access control. Never treat serial framing alone as cryptographic authentication.

**Not the general bus:** HDMI is designed around AV transport. A camera HDMI output may be captured through a dedicated interface, but HDMI should not be the participant-to-participant network.

Reference data-rate example from P02B: 1,020 channels × 20,000 samples/s × 2 bytes = 40.8 MB/s (~326.4 Mb/s) *raw* for one supported instrument configuration, before headers, storage and optical streams. One 1GbE link is nominally enough for that arithmetic but not guaranteed after real headroom, bursts and sharing. Qualify measured throughput; use dedicated USB3 or faster Ethernet for instrument paths.

## 11. Low-level diagnostic readout bus

Serial readout is *observation only* and available from the service processor, not a backdoor to stimulation/feeding/control. Diagnostic topics:
- BOARD_ID, HW_REV, FW_BUILD_DIGEST, BOOT_NONCE, UPTIME, BOOT_REASON, WATCHDOG_COUNT
- POWER_GOOD, supply rail condition, board temperature, link/carrier status, PHY errors
- PORT_ENUMERATION, DEVICE_CAPABILITIES, SERIAL_FRAMING_ERRORS, CRC_ERRORS, DMA_OVERRUNS, QUEUE_WATERMARK
- CLOCK_DOMAIN, LAST_SYNC_ANCHOR, OFFSET_UNCERTAINTY, CAPTURE_GAP_COUNT
- SENSOR_VALIDITY, LAST_SAMPLE_TIME, CALIBRATION_DIGEST, FAULT_LATCHED, INDEPENDENT_LIMIT_STATUS

Every readout is a typed WindPacket or an explicitly bounded link-frame mapped to it by the gateway. Include monotonically increasing sequence, reason/observed time, units when applicable, staleness and payload digest. For a UART service console, use a strict read-only command whitelist, audited access, no untrusted binary dump of restricted biological data, and physically gated maintenance mode.

## 12. Host/client plug-in onboarding

1. **Connect:** Ethernet carrier/PoE or separately rated power, or supported USB/fieldbus peripheral. Physical link alone grants no access.
2. **Discover in quarantine:** enumerate only non-sensitive board identity/capability metadata via a restricted bootstrap lane.
3. **Enroll:** operator binds cryptographic device key / attestation evidence to an approved device, site, colony, scopes and expiry.
4. **Negotiate:** WindPacket version, binary schemas, packet size, signed capability manifest, readout list, sampling/event policy and limits.
5. **Subscribe:** select explicit authorized channels (e.g. colony state, voltage, image metadata, acquisition gap) with service quality and byte budgets.
6. **Receive snapshot:** materialized colony revision plus current known metric freshness and encrypted/scoped evidence refs.
7. **Stream deltas:** callbacks push state changes and receipts immediately. Optional raw capture stream uses separate negotiated credits.
8. **Reconcile:** reconnect performs hash/sequence reconciliation and immutable journal catch-up; stale states stay visibly stale.
9. **Deregister:** revoke credentials/subscriptions, retain evidence history, and enforce a safe independent equipment state.

Host, MCU and observer share logical onboarding contracts, but not identical hardware runtime, credential storage or payload capacities. Adding a biological candidate does not enroll it as a trusted digital host.

## 13. Operational resilience and clocking

No application-level timed polling for data changes. Allow interrupt-driven acquisition, OS event completions, subscription pushes and blocking waits. Safety watchdog timers and heartbeat timeouts are independent and may be periodic. A snapshot is a local state read, not repeated background polling.

Use hardware capture triggers and PTP/IEEE 1588 or 802.1AS on capable Ethernet devices to align clocks; record the residual error and fall back to explicitly uncertain source-specific timing on unsupported MCU links. For critical fault handling the local hardware remains authoritative even when Ethernet, storage or Lotus fails.

A state replica may continue reporting last-known observations while isolated, always labeled with revision, source time, link health and STALE/UNKNOWN quality. There is no fiction of continuous biological viability from software liveness.

## 14. Feedback across colonies: improve the observer, not uncontrolled biological treatment

DevelopmentCase history = provenance of baseline -> observation -> predictive assessment -> reviewer decision -> approved study outcome -> independent retest. Compare candidate new **digital observer/planner** versions against withheld colonies and matched studies. Promote a model update through simulated replay, shadow mode, independent evaluation, signed changeset and rollback-supporting deployment. Protect existing competence and report negative/unknown results.

Never automatically transplant a biological intervention from one colony to another. Differences in preparation, assay, equipment, source material and lab approval create explicit compatibility gates.

## 15. Message and persistence contracts

Reference schema files:
- ../../schemas/software/colony_state.v1.schema.json
- ../../schemas/software/board_capability.v1.schema.json
- ../../schemas/software/bread_meat_plan.v1.schema.json
- ../../schemas/software/guidance_proposal.v1.schema.json

The JSON schemas are descriptive interchange/diagnostic records; production CE-SW-01 uses a versioned binary canonical form and content hashes with a tested mapping. A packet received or a proposal generated is not a PERSISTED journal entry or an approved physical action.

Minimum proposed events:
COLONY_STATE_REVISION, PARTICIPANT_CANDIDATE, BOARD_ENROLLED, BOARD_FAULT, SENSOR_OBSERVATION, SENSOR_DATA_GAP, SUPPORT_STATUS, CHEMICAL_OBSERVATION, BREAD_PACK_COMPILED, MEAT_PLAN_PROPOSED, LEARNING_TEST_RECORDED, NEED_IDENTIFIED, GUIDANCE_PROPOSED, GUIDANCE_REVIEWED, CLOCK_ANCHOR, STORE_CHECKPOINT.

## 16. Release and acceptance gates

**SIM0:** synthetic boards and colonies, mutable simulated observations, immutable event sources, revisioned snapshots and push-delta reconnect.
**SIM1:** Bread/Meat *digital-only* planner with frozen holdout tests, source/rights audit, protected old-skill regression checks and missing-data abstention.
**HW0:** A-B1-style dry-zone bench sensor board with verified UART/USB/Ethernet readouts, bounded memory, link-loss/fault tests, secure enrollment and firmware provenance.
**HW1:** qualified vendor camera/MEA acquisition, loss accounting, timestamp validation and throughput benchmarks with inert test sources.
**R0:** only after institutional review: read-only laboratory observation under existing OEM safety; no automated care/stimulation or physical Crystal claim.

Acceptance tests must show: stale sensor never presented as current; no replay of a physical action; accepted serial peer cannot gain actuation rights; no LWW erasure of contradictory evidence; subscription enforces scope; 1GbE overload produces a loss record; disconnected state still retrievable with age; faulty clocks abstain from causal temporal inference; model improvement beats held-out baselines instead of merely improving training scores.

## 17. Decisions and unknowns

Provisional recommendation: **Cat5e/1GbE as primary native mesh; USB3/GigE Vision for local optics; separate isolated fieldbus/USB diagnostic; SFP+ fiber later where required; never HDMI as general data bus.** Exact board connector, power budgets, physical isolation, safety approvals, RTOS/PHY, embedded key storage, throughput target and camera/MEA vendor compatibility remain controlled engineering decisions. No hardware commissioning or biological mechanism is demonstrated by this specification.

## References

- [BIO-04 SustiBus/ChemiBus provisions](../../processes/BIO-04-sustibus-chemibus.md)
- [BIO-06 education and retention](../../processes/BIO-06-teaching.md)
- [BIO-09 growth governance](../../processes/BIO-09-growth-control.md)
- [P02B-02 board topology](../../phases/P02B/docs/design/P02B-02_Board_Topology_and_Instrument_Interfaces_Design.md)
- [P02B-06 supervisor MCU](../../phases/P02B/docs/design/P02B-06_Supervisor_MCU_and_Firmware_Design.md)
- [CE-SW-01 Native Mesh](CE-SW-01_native_mesh_architecture.md)
- [CE-SW-02 Developmental WindBus](CE-SW-02_developmental_windbus_architecture.md)
- [USB-IF USB 3.2](https://usb.org/usb-32-0)
- [A3 GigE Vision](https://www.automate.org/vision/vision-standards/vision-standards-gige-vision)
- [IEEE 802.1AS time synchronization](https://standards.ieee.org/ieee/802.1AS/11968/)
