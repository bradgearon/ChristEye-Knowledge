---
title: "P02-02 WindBus Messages and Connective Controllers Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/design/P02-02_WindBus_Messages_and_Connective_Controllers_Design.docx`  
> **SHA-256 (original DOCX):** `a4a00214e023155202056c0ebe2866d8cd5763a7e504fd59300d0e6962a7bdb1`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Revision:** 1.0 \| **Owner:** software protocol lead \| **Runtime:** src/anaiero_p02/protocol.py, bus.py \| **Release mode:** simulated

# 1. Functional requirement

WindBus is the named communication fabric between the host H, living-core surrogate C, evaluator E, observer O, support policy S, recipient B, and independent limit controller L. It transports **typed digital descriptions of activity**; it does not assume a word, a packet, or an event is the same thing as an electrically delivered stimulus. A Wind is defined by its boundary, carrier, receiver, codebook and measured consequence, which prevents the software language from being mistaken for neural language. Every received event has source identity, event type, sequence, trace correlation, temporal validity and a delivery receipt. \[I01, I02\]

# 2. Envelope v1 contract

Envelope has fields event_id (unique immutable identifier), trace_id (end-to-end trial/campaign), source, recipient, event_type, sequence (monotonic per source/recipient pair), emitted_ns and deadline_ns (same declared clock domain), payload (bounded JSON object), consent_scope, schema_version, clock_domain, parent_id, and producer_digest. In v1, schema_version is 1.0.0 and clock domain is sim-monotonic. payload is copied and deeply frozen on construction; its canonical JSON digest identifies the exact message. Strict validation rejects NaN, unknown event families, non-positive intervals, absent identities, and physical-actuation payload fields. A digest detects changes when recalculated, but is not an author signature or proof of origin.

| **Event type**                  | **Expected source → recipient** | **Permitted content**                                    | **Forbidden inference or action**                               |
|---------------------------------|---------------------------------|----------------------------------------------------------|-----------------------------------------------------------------|
| TASK_CUE                        | H → C/B                         | Context, ordered symbols, GO reference and codebook ID   | No future target, stimulus amplitude or physical waveform       |
| TASK_RESULT                     | C/B → E                         | Frozen-reader class, validity and timing                 | Does not prove source stored a biological memory                |
| OBSERVATION                     | C/H → O                         | Pre-outcome permissible telemetry and missingness        | No evaluation answer, outcome flag or future-normalized feature |
| TRAINING_FEEDBACK               | E → C                           | Training-only declared consequence after response closes | Never delivered in BASELINE/CONFIRMATION                        |
| SUPPORT_PROPOSAL                | O/S → H                         | Candidate review, defer, alternate route or no change    | Cannot operate pumps, chemistry or physical outputs             |
| DECISION / RECEIPT              | H ↔ L                           | Requested logical action, independent allow/veto reason  | L not trained by task reward                                    |
| FAULT / HEARTBEAT / SYNC_ANCHOR | Instrument-host roles           | Fault state, liveness and clock evidence                 | Must not be interpreted as task labels                          |

# 3. Routing authorization and trust boundaries

Each Route declares one source, one recipient, an allowed set of event types and a maximum message size. An event is rejected if no route matches, the recipient lacks a subscriber, the event is too large, or the source's sequence does not advance. E is the only valid source for a TRAINING_FEEDBACK envelope. The host may request but cannot grant itself an L permit. The observer is initially shadow-mode; its advisory output is logged without changing C. Recipient B is a separate logical role and requires its own independent interface and removal test in P05. The route_acl.csv is the authoritative configuration source.

# 4. Delivery and failure semantics

P02 uses a bounded in-process queue. It is **not** a distributed exactly-once broker. enqueue() may return ACCEPTED, REJECTED, EXPIRED or DUPLICATE. ACCEPTED means queued, not executed. drain() returns DELIVERED if the subscriber completed, EXPIRED if time elapsed, or HANDLER_ERROR with a dead-letter entry. A failed handler is **not silently retried**, because repeated teaching events could alter a learner; the caller must produce a new, explicitly auditable event after review. Sequence and deduplication are kept in memory only, so restarts require a new session ID. Production transport will need durable queues, replay rules and signed policy after P02.

# 5. Concrete message and procedure

Example logical message (all numbers refer to software time, not hardware waveforms):

{"event_id":"evt-104","trace_id":"run-C01","source":"H","recipient":"C","event_type":"TASK_CUE","sequence":104,"emitted_ns":5000000000,"deadline_ns":5100000000,"payload":{"context":"C0","first":"A","second":"B"},"consent_scope":"local","schema_version":"1.0.0","clock_domain":"sim-monotonic","parent_id":null,"producer_digest":"development-only"}

H checks local session state → L checks permitted logical operation → H enqueues task → C responds through frozen reader → E records validity and label privately → E releases feedback only if split is TRAINING → H writes requested versus delivered receipts. Correlation IDs must connect request, actual delivered event, response, label release, and final trial. A timeout produces an invalid opportunity rather than a fabricated HOLD.

# 6. Tests and release gates

Unit tests verify repeatable canonical hashing; deep immutability; unknown event rejection; evaluator-only feedback; ACL; no subscriber; duplicate, out-of-order, expired and oversized messages; queue backpressure; and handler failure to dead-letter. P02 claims only logical message integrity and routing behavior on one process. Later service-to-service transports must demonstrate authentication, authorization, ordering boundaries, queue recovery and high-percentile latency separately. \[I01\]

# 7. Source anchors

\[I01\] CE-DEV-001 v0.4 §5, 13–15, 21–22, https://www.mxwbio.com/products/maxone for downstream device identity. \[I02\] CE CRYSTAL MATH 001 audit, content-vs-carrier and causal distinctions. \[N01\] NWB standard for eventual time-series indexing, https://nwb-schema.readthedocs.io/en/latest/format.html.

