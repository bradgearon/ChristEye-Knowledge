---
title: "P02-02 WindBus Messages and Connective Controllers Companion"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/companion/P02-02_WindBus_Messages_and_Connective_Controllers_Companion.docx`  
> **SHA-256 (original DOCX):** `1b19f812ff41b6463ed2730489cb786e6a9e467065754ee4d6186a067a49e22d`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Evidence classification:** D = implementation; H = later living interface hypothesis; F = scoped published finding. Nothing in the bus source establishes H.

# 1. A digital signal is not a biological memory

Wind is a functional descriptor in the ChristEye architecture. P02 operationalizes it as an event with source, recipient, codebook, content and timestamps. It may describe the network state, contain a task cue, report an observation, or carry a teacher's label. These are different information budgets. A valid packet can transmit correct answers even if the receiving network never learns them; therefore task scoring while the bridge is attached is assisted access, not independent retained acquisition. This is why the controller records bridge direction and why P05 must evaluate B alone after disconnect and cache clearing. \[I01\]

# 2. Codebooks and recipient-specific language

The reference truth table uses C0/C1 and A/B tokens. On the software side, a JSON codebook relates these symbols to logical cue IDs and response labels. For a future biological adapter, an actual observation and input profile must be separately validated; the same symbols do not have an innate cellular meaning. The receiver-specific mapping must be measured with correct alternatives, invalid inputs, missing observations and explicit UNKNOWN. A stimulus that drives a large response without differentiating A vs B can pass a responsiveness check yet fail the information interface. The receiver's trained codebook and its digest are therefore part of every trial. \[I01\]

# 3. Channel and payload capacity

An envelope's byte size is evaluated after canonical JSON encoding. max_bytes prevents an accidental full raw recording from entering a low-latency control queue. Raw streams are referenced by content address, byte range and clock interval; they are not copied into every WindEvent. In a later deployment, transport performance should be measured at sustained and burst loads, including tail latency, loss, duplicate delivery and queue backlog. A microsecond claim cannot be inferred from Python's callback speed or from a vendor's nominal sample period.

# 4. Sequence, time and exactness

At-most-once P02 callback delivery follows from in-process event-ID deduplication and no automatic handler retries. It is not durable exactly-once behavior: a process crash after a subscriber effect but before durable receipt would create ambiguity. The production path needs an idempotent command key, atomic effect/receipt strategy where possible, and an explicit replay policy that does not issue physiological stimulation on uncertain history. Expiration rejects events outside their declared deadline only if the associated clock mapping is valid. Failed synchronization should veto actions rather than make stale packets appear timely.

# 5. Security and provenance limitations

A SHA-256 digest is suitable for identifying an unchanged canonical payload, not for proving who sent it. Production messages between workstations would additionally require authenticated endpoints, least-privilege roles, message-signature or transport-authentication controls, protected source material, revocation, and independent audit. consent_scope=shareable is metadata rather than blanket permission to export genetic, medical or private project data. The central router needs separate provenance and authorization checks before importing externally sourced instructions or CrystalPacks. NIST CSF 2.0 provides a governance taxonomy, but does not prescribe these concrete controls. \[I01\]

# 6. Tests that differentiate routing from learning

An isolated communication test compares the intended message, transmitted bytes, recipient's observation, and delivery receipts. A learning test instead compares later responses with all teacher/answer channels absent, against a matched noncontingent and digital-teacher condition. Correlated activity between two modules is not automatically transmission: shared input or shared support can produce synchrony. The bridge's intervention-supported role should be distinguished from statistical co-activity and structural contact in separate Mesh graph edge types. \[I01, I02\]

# Additional engineering annex: message-sequence scenarios

**Case A — duplicate task event.** H transmits evt-17 for trial T17 and receives ACCEPTED but loses the delivery receipt. Repeating evt-17 returns DUPLICATE, not a new TASK_CUE, within the same process. Creating evt-18 would be a **new** experiment exposure; it must be marked as such, not called a transport retransmission. In an actual living system, uncertainty about whether a cue was delivered can affect plasticity, so a later distributed queue needs a reconciled transaction policy rather than automatic stimulation retry.

**Case B — teacher information contamination.** A bridge from module A carries {predicted_class:X, confidence:0.9} to B. This message includes possible task-answer information, even if its payload is called an 'observation.' For independent acquisition, B must later work with its own encoder/decoder and no A-derived cache. A replay of A's response is a valid teaching control but not an information-free baseline. The bridge's declared information budget matters as much as its byte budget. \[I01\]

**Case C — lost clock quality.** A message is inside its stated nanosecond deadline, but the clock-domain fit no longer qualifies. The correct disposition is INVALID TIMING and L VETO, not delivery based on a numerical comparison between unaligned domains. clock_domain and clock_fit_id must travel with any cross-device event. A message from the Internet should never be allowed to construct a physical-device command without an independent, separately authenticated adapter and limit approval.

**Migration constraint.** The existing v3 WindMessage.kind='task' maps to P02 TASK_CUE only when the source/recipient and payload codebook are compatible; a textual field name resemblance is not enough. A contract migration should be tested with fixtures containing missing recipients, stale scopes, nested data and revoked consent. Production authentication, persistence and acknowledgment remain unsolved beyond this local reference.

# 7. Scope references

\[I01\] Physical Learning Device v0.4 §§18,20–21. \[I02\] ChristEye Technical Companion/Mathematical Reconciliation §§role mappings and information. \[N01\] NWB time-series spec, https://nwb-schema.readthedocs.io/en/latest/format.html. \[V02\] MaxLab Live API, https://api-docs.mxwbio.com/index.html. These references support interface concepts, not a validated Anaiero biological codebook.

