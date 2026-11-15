---
title: "CE-SW-01 — Anaiero Native Mesh, WindBus and Evidence Changesets"
project: "ChristEye Anaiero"
status: "Architecture proposal — not an implemented or validated production runtime"
version: "0.1.0"
date: "2026-11-15"
owner: "Software architecture / We Develop Software LLC"
classification: "software-only / read-only biological interfaces unless independently qualified"
---

# CE-SW-01 — Anaiero Native Mesh

**Design purpose:** A single portable, cryptographically authenticated, event-driven information fabric from embedded instrumentation nodes through laboratory hosts and specialist StarChips to data-center clusters. It carries observations, evidence, immutable changesets, capability announcements, and delivery receipts without conflating digital delivery with biological stimulation or learning.

**Status:** Proposed software contract. The existing Python WindBus and store are digital references, not this distributed implementation. Biological learning, transferable engrams, physical Crystals, or autonomous biological modification are **not** established by this design.

## 1. Authority and compatibility with the existing repository

This proposal extends rather than replaces:

- `specifications/v3/master_architecture.md` (eight planes and A11/A19/A20/A22 separation)
- `specifications/v3/design/CE-A11_connective_controllers_Design.md` (typed routes, ACL, TTL, receipt, idempotency)
- `specifications/v3/design/CE-A19_central_data_router_and_host_Design.md` (source identity, quarantine, permissions)
- `specifications/v3/design/CE-A20_filesystem_and_snapshots_Design.md` (immutable content and snapshots)
- `phases/P02/docs/design/P02-02_WindBus_Messages_and_Connective_Controllers_Design.md` (logical WIndEnvelope v1, simulated event kinds, never automatic replay of teaching)
- `phases/P02B/docs/design/P02B-09_Neural_Acquisition_and_Multimodal_Time_Base_Design.md` (vendor instrument boundary, raw acquisition clocks, drop receipts)
- `phases/P02B/docs/design/P02B-06_Supervisor_MCU_and_Firmware_Design.md` (observe-only by default and physical isolation)
- `schemas/v6/wind_envelope.schema.json` and `schemas/p02/wind_envelope.v1.schema.json` (logical schema baseline)
- `anaiero/bus.py` and `anaiero/store.py` (non-distributed Python test/reference semantics)

The **logical WindEnvelope** is distinct from the **binary WindPacket** that transports it. Neither replaces the existing JSON schemas during transition. New binary fields map to logical fields by a versioned schema registry. Preserve evidence classes F (source finding), X (cross-source interpretation), H (hypothesis), D (engineering decision), A (analogy).

## 2. Non-negotiable invariants

- **No application-level polling for new messages:** every channel delivers through interrupt, DMA completion, OS readiness/completion API, or transport callbacks. Peer changes are announced by push subscription; reconnection performs event-triggered reconciliation. Liveness heartbeats and watchdog timeouts are permitted, as is an event-loop blocking wait. No busy-wait loops or timed database fetches for new work.
- **One semantic bus, multiple lanes:** reliable control/evidence and high-bandwidth acquisition have separate physical queues and congestion budgets, but shared identity, routing, schema, provenance, and trace semantics.
- **Authenticate and authorize before accepting:** unknown peers remain in quarantine. A successful cryptographic handshake proves possession of a key, not permission to publish, subscribe, stage or actuate.
- **Write-ahead durability:** if a publisher requests durable delivery, append the immutable record to its committed local journal before declaring `PERSISTED`.
- **No distributed exactly-once claim:** at-least-once delivery with deduplication and idempotent digital application. Explicitly never retry a biological intervention or teaching event as if it were an ordinary idempotent digital message.
- **Full causal records:** immutable source identity, scope/consent, schema, monotonically increasing source stream sequence, message/event ID, local clock domain, recorded uncertainty, content digest, causal parent references, processing receipts and software/calibration digests as appropriate.
- **Safety authority is independent:** A22/instrument interlocks retain fail-closed authority. Distributed Lotus decisions are proposals, not direct actuation permits. A disconnected host cannot issue a synthetic permit.
- **Evidence is not truth:** a signed message proves the signer committed to bytes. A digest proves integrity of those bytes. Neither proves biological interpretation, task success, or a valid scientific claim.
- **Fail visibly:** overflow, missing samples, stale clocks, unsupported schemas, rejected identities, incomplete replication, expired deadlines and storage faults create explicit status and loss records.

## 3. Network hierarchy and execution environments

```text
                       Remote authenticated peer clusters
                        +------------------------------+
                        | Replication / object store   |
                        | Policy authority / indexing  |
                        | StarChip workers / archive   |
                        +---------------+--------------+
                                        | QUIC/TLS 1.3
+---------------------------------------+-----------------------+
| Laboratory host / Linux or Windows / Anaiero Cluster Node     |
| A19 SourceRouter / A11 MeshRouter / A20 WindStore / A12 Lotus  |
| WindScheduler / WindChangeLedger / A24 Observers / A22 bridge   |
|     reliable events       raw stream lanes       object sync    |
+---------------+-------------------+--------------------------+
                | authenticated local gateway
     +----------+-------------+------------+----------------+
     |                        |                             |
  Supervisor MCU          Vendor MEA bridge          Other sensors
  interrupts/DMA          vendor-qualified API      calibrated IO
  compact WindPacket      acquisition chunk stream   compact event
  observe only            independent clock          observe only
```

### 3.1 Deployment profiles

| Profile | Hardware/runtime | Binary compatibility | Responsibilities | Explicit limits |
|---|---|---|---|---|
| **M0** | freestanding C++20 MCU, RTOS optional | Core wire format + signed metadata | bounded telemetry sender, identities, local sequence, fault/loss receipts, direct wired link | no SQL, no heap required, no QUIC requirement, no remote actuation |
| **G1** | small Linux gateway or robust host interface | full WindPacket and schemas | durable ingress, Noise or secure-device-channel termination, validation, forwarding, backpressure | does not substitute for OEM safety |
| **H2** | Windows/Linux native host | full protocol | local commit store, journals, Lotus, specialized services, acquisition, digital evaluation, endpoint scheduling | external actions require independent authorization |
| **C3** | clustered Linux/Windows server | full protocol | replication, partitions, query/index, policy/identity, StarChip jobs, immutable archives | no direct control over independent physical limits |

Same C++ **source contracts** run everywhere, but not the same binary or dependency footprint. Shared headers must be header-only/portable or freestanding; platform adapters are separately linked.

### 3.2 Processes and ownership

`CaptureAgent` owns timestamped device ingress; `WindRouter` owns routing and ACL; `WindScheduler` owns per-endpoint queues; `WindStore` owns immutable objects and journals; `WindChangeEngine` owns proposal/commit/merge; `MeshSession` owns authenticated peer sessions; `PolicyGate` owns software authorization; `EvidenceIndexer` produces derived query views; `LimitBridge` can only convey independently issued decisions/receipts. A24 observer consumers cannot gain actuator privileges through a routing alias.

## 4. Native wire contract: WindPacket/1

**Canonical byte order:** little-endian for numeric fixed fields; no raw C++ struct layout is serialized. All parsers must validate length before allocating or accessing payload. Bounds are part of the negotiated profile. Protocol version `1.0`; unknown major versions are rejected, unknown OPTIONAL TLVs skipped, unknown REQUIRED TLVs rejected.

### 4.1 Fixed 56-byte header

| Offset | Size | Field | Notes |
|---:|---:|---|---|
| 0 | 4 | magic | `CEWP` ASCII |
| 4 | 1 | major | `1` |
| 5 | 1 | minor | `0` |
| 6 | 2 | flags | encrypted-link, compressed, chunked, required-TLV indicator, no-replay |
| 8 | 2 | header_bytes | includes TLV extension bytes, maximum negotiated |
| 10 | 1 | kind | OBSERVATION, EVIDENCE, CHANGE, RECEIPT, CAPABILITY, CONTROL, STATUS, OBJECT, SYNC |
| 11 | 1 | delivery_class | A,B,C,D,E below |
| 12 | 4 | source_slot | session-local mapping to authenticated principal |
| 16 | 4 | destination_slot | session-local mapping to authorized principal/route |
| 20 | 4 | stream_id | source stream/lane; independent sequencing |
| 24 | 8 | sequence | monotonically increasing per authenticated source-epoch/stream |
| 32 | 16 | message_id | UUIDv7-compatible 128-bit or monotonic source-local unique ID |
| 48 | 4 | payload_bytes | bounded by negotiated profile |
| 52 | 4 | schema_id | registry ID scoped by protocol version |

`header_bytes + payload_bytes` MUST equal the authenticated application frame length, excluding any link-layer protection/framing. This is a **design proposal**, not an existing wire ABI. Session slots are never globally trusted identifiers: cryptographic session identity resolves them. Large streaming frames use separately bounded `OBJECT`/acquisition chunks with offset and per-chunk digest.

### 4.2 Canonical extension TLVs

Each TLV: `type:u16`, `length:u16`, `value[length]`; sorted ascending by `type` only for **canonical signed records**, not necessarily for transient in-memory event processing. Reject duplicate singleton TLVs. V1 planned TLVs: `EVENT_TIME`, `CLOCK_DOMAIN`, `CLOCK_UNCERTAINTY`, `DEADLINE`, `PAYLOAD_SHA256`, `PARENT_IDS`, `TRACE_ID`, `PERMISSION_SCOPE`, `CALIBRATION_DIGEST`, `PRODUCER_BUILD`, `STREAM_EPOCH`, `FRAGMENT`, `SIGNATURE_REF`, `PROVENANCE_REF`, `ENCODING`, `OBJECT_ROOT`. Canonical signed metadata is length-delimited and domain-separated (e.g. `CE/WINDCHANGE/1\0`).

Payload types are chosen **per schema ID**, not guessed from incoming bytes. Small MCU payloads use bounded field-tagged TLVs. Larger immutable records use deterministic canonical CBOR or a versioned schema format after conformance tests. External JSON is a diagnostic/export representation, not the network ABI. Do not hash a noncanonical serialization and call it a stable object ID.

### 4.3 Fragmentation and media constraints

USB/UART/CAN-FD gateways may fragment link frames; a receiver reassembles only under a negotiated byte budget and monotonic deadline. Per-fragment IDs and final object hashes detect missing or mixed pieces. A 56-byte application header is too large to use as a complete 64-byte CAN-FD data frame, so the CAN transport must use dedicated compact link framing and reassemble the application packet at the gateway. MCU profiles may publish reference-and-digest metadata rather than copying bulk waveform data through narrow links.

## 5. Message classes and delivery contracts

| Class | Example | Delivery | Durable? | Deadline/handling |
|---|---|---|---|---|
| **A: fault/safety evidence** | limit fault notice, watchdog reset, sensor loss | reliable local fault path + auditable event | yes where storage remains available | local independent limit acts without waiting on network |
| **B: decisions/receipts** | policy decisions, control requests, immutable ACKs | reliable ordered stream per key | yes | reject if stale; requests are never silently executed again |
| **C: scientific acquisition** | waveform chunk, optical frame reference, assay observation | high-throughput lane; backpressured or explicit loss receipt | raw archive yes | never silently downsample/drop source evidence |
| **D: changesets/evidence** | WindChange commits, claims, verification reports | reliable store-and-forward, hash verified | yes | retry transfers safely; application is idempotent |
| **E: transient UI/status** | visual monitoring, progress/status | opportunistic datagram or event push | optional | expired states may be dropped with counters |

**Important:** QUIC DATAGRAM frames are secure but **unreliable**. Use QUIC reliable streams for evidence, changes and receipts, and QUIC datagrams only for declared lossy status. The bus must not misrepresent datagram delivery as a confirmed archival write.

## 6. The event-driven transport layer

### 6.1 Host-to-host / cluster-to-cluster

QUIC over UDP with TLS 1.3, mutual endpoint authentication, application-level authorization, stream priorities implemented above transport and negotiated datagrams for transient lanes. Prefer tested native C/C++ implementations through an abstract transport interface (e.g. MsQuic where platform support is suitable). TLS 1.3 0-RTT data MUST NOT be accepted for state-changing requests, changeset installation or non-idempotent messages due to replay concerns.

### 6.2 MCU-to-gateway

UART/USB/CAN-FD, interrupt/DMA driven, using a formally specified secure channel with a vetted cryptographic library. A Noise XX-based join can establish an encrypted authenticated session **only after** the discovered static public key is bound to an authorized device record. Provisioned PSK or existing hardware secure-element identity may be appropriate in constrained environments; production choice needs hardware review. Never implement new encryption primitives. An insecure USB cable does not imply an authenticated peer; use independent channel authentication where the threat model requires it.

### 6.3 Event loops and lifecycle

- Windows: IOCP/completion-based transport and I/O adapters.
- Linux: epoll or io_uring completion-based adapters; high-bandwidth capture may use DMA/vendor callbacks.
- Embedded: ISR posts a bounded descriptor into a lock-free or short critical-section ring; a task drains it on notification/semaphore (not timed scans). ISRs never parse untrusted full packets or run cryptography.
- Join sequence: `DISCOVERED -> UNTRUSTED -> HANDSHAKE -> IDENTITY_VERIFIED -> CAPABILITY_NEGOTIATED -> POLICY_AUTHORIZED -> SUBSCRIBED -> ACTIVE`. Quarantine on unknown identity, revoked authority, unsupported schema or clock invalidity.
- Reconnection: transport completion/event triggers a signed `SYNC_HELLO` with stream high-water marks, content roots, changed causal tips, and quotas. Peer responds with signed missing ranges and object requests. Reconciliation is event-driven, not scheduled polling. A missed heartbeat may trigger disconnect/reconnect, but does not authorize intervention.

## 7. Local first: WindStore

`WindStore` stores **immutable content-addressed blobs**, **append-only stream journal segments**, **signed checkpoints**, **indexed views**, and **application state snapshots**. The canonical source of truth is bytes plus lineage; indexes can be rebuilt.

Proposed logical collections:

```text
objects/{sha256-prefix}/{sha256}    # immutable payloads and manifests
journal/{producer}/{epoch}/{seg}    # append-only records with per-record digest
checkpoints/{scope}/{seq}           # signed Merkle/head commitments
changes/{change_id}                 # immutable WindChange manifests
receipts/{message_id}/{recipient}   # durable lifecycle receipts
snapshots/{snapshot_id}             # logical path -> content object hash
indexes/...                         # derived: can be discarded/rebuilt
```

MCU flash implements a bounded ring with record integrity, wear-aware write strategy and overrun status; a large host uses an embedded transactional engine for metadata/index (e.g. RocksDB or SQLite as selected by benchmarks) plus sequential files/object store. Never claim `PERSISTED` on successful queue insertion alone. `fsync`/durability policy must be explicit for each class and tested under power loss.

## 8. WindChange: small evidence-linked immutable changesets

A WindChange is **not raw neural state**. It is a proposed or validated digital artifact with causal lineage. Small edits can describe a corrected sensor annotation, calibration metadata, codebook, Root candidate, capability manifest, curriculum adjustment, or signed firmware/software release.

**Canonical manifest fields:**

```text
WindChange/1 {
  change_id = SHA256(canonical_manifest_without_signatures),
  parents: [ChangeId],          // causal DAG, not necessarily linear history
  actor_principal: PrincipalId,
  source_endpoint: EndpointId,
  source_stream_epoch: u64,
  source_sequence: u64,
  proposed_at: TimeWithDomainAndUncertainty,
  target_namespace: string,
  operations: [PutObject | TombstoneReference | SetVersionedReference],
  base_snapshot: Optional<SnapshotId>,
  content_objects: [Digest],
  evidence_refs: [EvidenceId],
  permission_scope: ScopeId,
  schema_id: SchemaId,
  claimed_effect: Optional<Statement>,
  qualifications: [TestOrReviewRef],
  author_signature: Signature,
  release_approvals: [ApprovalRef] // separate from author's signature
}
```

The `change_id` uses a canonical manifest that excludes all signature and transport-ephemeral fields, and signatures bind its ID plus its domain-separated scope. Merging two branches produces a **new manifest with both parent IDs**; old history is never edited. Conflicts in annotations can use explicit multi-value records; conflicts in permission, firmware, policy, biological interpretation and physical limits require the designated authority and **cannot** use last-writer-wins.

### 8.1 Changeset state machine

```text
LOCAL_CANDIDATE
 -> EVIDENCE_ATTACHED
 -> VALIDATED (schema, source lineage, rights, clock, policy)
 -> DURABLY_COMMITTED
 -> ANNOUNCED to authorized peers
 -> PEER_FETCHED_AND_VERIFIED
 -> PEER_STAGED
 -> REVIEWED/APPROVED if required
 -> APPLIED to named digital namespace
 -> INDEPENDENT_RESULT_RECORDED
```

Each arrow is a signed/traceable receipt, and each state is separately addressable. A peer may archive a changeset without installing it. `REJECTED`, `EXPIRED`, `SUPERSEDED`, `QUARANTINED` and `ROLLBACK_REQUIRED` are explicit terminal/transition records. A change that suggests a physical action is still just data: it cannot bypass A22, firmware checks or site review. A biological system's inferred intention is an interpretation with a decoder/model version and evidence, never intrinsically authenticated as an instruction from a neuron.

### 8.2 Minimal evidence record

```text
EvidenceRecord/1 {
  evidence_id, class: F|X|H|D|A,
  observation_or_claim, producer_principal,
  acquired_time, acquired_clock_domain, time_uncertainty,
  raw_source_digest, transform_digest, codebook_digest,
  instrument_id, calibration_digest,
  conditions_digest, missingness_or_loss_refs,
  causal_parent_ids, access_scope,
  evaluation_protocol_ref, signature_ref
}
```

The record separates **raw observation**, **processing/interpretation**, **evaluation**, and **decision**. Use lineage refs to link across independent producers; the index may build a graph with relationships `DERIVED_FROM`, `TESTED_BY`, `CORRELATES_WITH`, `CONTRADICTS`, `APPROVED_BY` and `REPRODUCED_BY`. These are claim labels, not proofs of causation.

## 9. Unified addressing, subscriptions and route authority

Addressing is hierarchical with immutable numeric IDs mapped from a registry:

```text
anaiero/{domain}/{node}/{subsystem}/{channel}/{schema.major}
anaiero/labA/host01/a11/receipts/1
anaiero/labA/mcu07/sensors/temperature/1
anaiero/labA/mea01/acquisition/electrode-batch/1
anaiero/labA/host01/a16/roots/evidence/1
anaiero/labA/host01/a10/crystal/manifest/1
```

Prefixes are merely selectors; authorization is based on **authenticated principal + explicit capabilities** (publish, subscribe, forward, commit, stage, approve, administer) constrained by schema ID, namespace and access scope. The route registry is versioned, signed and cacheable with expiry. No unauthenticated wildcard subscription or global broadcast of restricted evidence. Broadcast means policy-filtered fan-out to interested authorized subscribers; it is not a network-wide plaintext flood.

Peers announce interest/subscriptions, then receive pushed updates only when authorized. Subscription changes produce events. Content broadcasts carry compact change IDs/manifests first; objects are transferred only to entitled peers that need them. This avoids sending high-bandwidth data to every node.

## 10. Weighted scheduling, queues and backpressure

**Three layers:** (1) fixed safety boundaries and dedicated resources; (2) admission control with quotas and credits; (3) fair scheduling within a lane. No score, including scientific confidence, can override safety gates or turn raw evidence into a command.

- Per endpoint: fixed-capacity queue(s) with memory/byte ceilings, source rate budgets, transport credit, and a reserved fault/receipt capacity.
- Delivery class A is handled by separate fault/limit paths and auditable events, never starved by acquisition traffic.
- Classes B/C/D use weighted deficit round robin (WDRR) or weighted fair queueing (WFQ); within a class with deadlines use earliest-deadline-first after admission, always bounded by per-route rate limits.
- Class E has drop/expiry semantics by contract; on overload increment signed counters and status summaries.
- Priorities are derived from an **authorized policy**, not untrusted publisher-supplied weight. Subscribers may express weights inside policy bounds.
- Persist/credit acknowledgment occurs at explicit stages: `TRANSPORT_RECEIVED`, `PERSISTED`, `VALIDATED`, `APPLIED`, `OBSERVED`. None is interchangeable with a biological response.

For weighted fair queueing, the ideal virtual finish time is

\[
F_{i,k}=\max(V(a_{i,k}),F_{i,k-1})+\frac{L_{i,k}}{w_i},
\]

where \(L_{i,k}\) is the packet's byte cost, \(w_i>0\) its authorized endpoint/channel weight, and \(V\) virtual service time. Use a bounded aging/deficit mechanism plus per-tenant minimum service to prevent starvation. **Priority affects when evidence is sent, never how credible it is.** Scientific evidence/confidence is stored as provenance metadata, independently from scheduling.

Backpressure is end-to-end: sensor DMA ring -> acquisition buffer -> gateway credit -> host durable store -> remote replication credit. Overflow anywhere emits an attributable loss record; lossless commitments require sufficient local spool space, verified throughput and an explicit failure policy. Do not block the MCU's high-priority interrupt indefinitely on network congestion.

## 11. Biological acquisition is a first-class event source, not a special ad-hoc bus

For vendor-supported sensors and MEA systems, every sample batch has stable `source_id`, channel map, raw-sample format/units, hardware sample range, first-sample timestamp, clock domain, timebase fit/uncertainty, calibration hash, data hash, transformation hash, gap counters and source software version. The raw sample data is stored in chunked objects over a high-throughput lane; evidence/control subscribers receive only metadata and content-addressed refs by default.

Differentiate at least: `requested_time`, `vendor_delivered_time`, `MEA_acquired_time`, `camera_exposure_time`, `MCU_monotonic_time`, `host_received_time`. Synchronize where possible with hardware trigger/PTP and retain residual uncertainty. Never reinterpret host arrival time as sensor acquisition time.

Illustrative recording rate from current P02B document: 1,020 channels × 20,000 samples/s × 2 bytes = **40.8 MB/s raw** prior to timestamps, headers, replication or camera streams. At that input rate, a 10 Gbit/s uplink can be ample in raw bitrate terms but storage endurance, copy overhead, compression CPU, high-percentile latencies and actual vendor link constraints must be benchmarked. The mesh must support metadata-only fan-out and contiguous archive streaming rather than duplicating raw signal to every subscriber.

## 12. Security and secure joining

Threats: impersonation, malformed frames, wire replay, unauthorized subscription, compromised software, stale/replayed permits, exfiltration of restricted research data, tampering with historical records, clock spoofing and denial of service.

**Join:** factory/site registration of immutable hardware identity or public key -> out-of-band operator authorization/invitation -> mutually authenticated encrypted channel -> signed capability and schema negotiation -> policy/ACL check -> bounded subscription admission -> signed join receipt. Unknown IDs may discover a join endpoint but cannot receive restricted state. Revoke at the principal level; cached credentials have explicit expiry. Private keys should live in OS-protected storage or a hardware secure element where available.

Use vetted TLS 1.3/QUIC on hosts; use a reviewed Noise-based or equally well specified embedded secure-channel implementation on constrained links. Distinguish encrypted transport MAC, application record signature, object hash and signed release approval. A compromised authorized publisher can produce authentic false claims; independent evaluation remains necessary. Pinning/trust-anchor policy and auditable key rotation are mandatory. 0-RTT must never authorize state mutation. Binary parsers must be fuzzed and memory-safe-by-construction as far as C++ constraints allow.

## 13. Replication and partition behavior

Immutable `WindChange` DAGs and objects replicate independently with hash verification and signatures. Peer membership and subscription state are policy-owned and versioned. Replication is incremental via per-stream committed high-water marks plus Merkle/root comparisons after a pushed change announcement or a reconnect event. Avoid periodic full-store rescans or unconditional object rebroadcast.

- **Local-first:** a valid local changeset can be committed while disconnected; its dissemination status remains `PENDING_PEERS`.
- **Partition tolerance:** archive and sensory observation can continue with bounded local storage. External authority and physical-action permissions do not get fabricated during partition.
- **Ordering:** preserve per-source stream order and causal dependencies; do not promise one global order for all sensors.
- **Conflict handling:** immutable commits coexist until a merge; one authority per sensitive mutable namespace; optional Raft-backed strong consistency only for replicated control-plane metadata in clusters where it is actually required.
- **Catch-up:** missing events/objects are requested by exact hash after event-driven state reconciliation; verify each object before acknowledgement.
- **Retention:** access rights and revocation policy govern export; retention and deletion rules must accommodate privacy/rights. An append-only record may need a revocable reference or encrypted object-key destruction, not unrestricted retention of protected personal information.

## 14. Native C++20 packaging

```text
src/
  ce_core/         # result/error, bounded spans, clocks, IDs, allocator contracts
  ce_wire/         # WindPacket encode/decode, canonical TLV, schema mapping
  ce_crypto/       # transport-independent authentication/signature API wrappers
  ce_event/        # WindEnvelope logical model, trace and evidence refs
  ce_queue/        # bounded SPSC/MPSC rings, WDRR + credits + deadlines
  ce_transport/    # pluggable transport API, session lifecycle
  ce_mesh/         # discovery, joins, identity/ACL, routing, subscriptions
  ce_store/        # journals, content addressing, manifests, snapshots
  ce_change/       # WindChange DAG, validation, merge, promotion/rollback
  ce_acquire/      # read-only acquisition/source adapters, clock alignment
  ce_policy/       # capabilities, independent limit bridge
  ce_observe/      # structured traces, signed receipts, metrics
platform/
  mcu/             # freestanding, UART/USB/CAN, RTOS/direct IRQ adapters
  windows/         # IOCP / native I/O
  linux/           # epoll/io_uring / native I/O
  cluster/         # replicated metadata, object backend, QUIC services
tests/
  vectors/         # golden binary frames, signatures, hashes
  property/        # parser invariants, queue fairness, state machines
  failure/         # drop, reorder, crash, disk-full, clock drift, partition
  integration/     # full local network and simulated sensor playback
```

**Build profiles:** `CE_PROFILE_MCU` freestanding C++20 (`-fno-exceptions -fno-rtti`, bounded fixed buffers, static allocations on critical paths), `CE_PROFILE_HOST` C++20 with std library, `CE_PROFILE_CLUSTER` native host plus replicated backends. No dependency on `std::filesystem` or heap allocation in MCU wire parser. Do not use packed struct casts for untrusted input. Use `std::span`, bounded integer operations, checked offsets and explicit endian decoding. Native CPU assembly is the compiler's output, not a handwritten protocol dependency.

**Stable API sketches (conceptual):**

```cpp
struct ByteSpan { const std::byte* data; std::size_t size; };
struct MutableSpan { std::byte* data; std::size_t size; };
struct Result { Error error; std::size_t bytes; };

Result decode_windpacket(ByteSpan encoded, WindFrameView& out) noexcept;
Result encode_windpacket(const WindFrameView& frame, MutableSpan out) noexcept;

class ITransport {
public:
  virtual void async_send(EndpointId, ByteSpan, CompletionToken) noexcept = 0;
  virtual void subscribe(ChannelId, ReceiveCallback) noexcept = 0;
  virtual void close(EndpointId) noexcept = 0;
};

class IWindStore {
public:
  virtual CommitReceipt commit_append(const DurableEvent&) noexcept = 0;
  virtual ObjectDigest put_immutable(ByteSpan) noexcept = 0;
};
```

Use vtables only in host profiles if chosen; embedded can use compile-time templates or function tables to avoid dynamic allocation and RTTI. `async_send` must define buffer lifetime, completion semantics, cancellation, credit return and ownership. The above interfaces are not compiled code or yet guaranteed ABI-stable.

## 15. Verification plan and nonfunctional targets

Define SLOs only after a representative instrument and hardware profile is chosen. Record p50/p95/p99/p99.9 latencies and worst-case queue residence **by class and hardware profile**, not a single system-wide mean. Mandatory metrics: lost sample ranges, bytes buffered, saturation, credit stalls, false permits (target zero), unauthorized publishes/subscribes (target zero), hash verification failures, clock drift/uncertainty, durable commit lag, replication lag, restore success and queue fairness.

**Required tests before any production claim:**

1. Golden binary vectors decode bit-identically on Cortex-M, x86-64 Windows and Linux, and an ARM64 host. Mutated lengths and unknown required TLVs reject without OOB access.
2. Property/fuzz tests reject invalid inputs, overflows, duplicate IDs, cyclic lineage, signature failure, torn writes, incompatible schemas and encrypted-message replay.
3. With two independent peers and a disconnected interval, local commits persist and converge after reconnection; no double-apply occurs.
4. Under sustained high-rate simulated sensor load, acquisition and control/status lanes retain reserved capacity and produce explicit loss records if overloaded.
5. A weighted scheduler gives minimum service to each admitted class and does not starve evidence under continuous control traffic.
6. Host crashes after journal append but before ACK: after restart, downstream idempotent digital apply is at most once, with audit receipts reflecting repeated delivery.
7. Client disconnect in the middle of an object/chunk yields an incomplete object, never a valid content hash or fictitious completed delivery.
8. Unknown/revoked clients cannot join restricted channels, approve changes or mutate configuration; invalid timestamps/calibration are rejected or quarantined.
9. Raw signals, decoder versions, source and clock uncertainties remain traceable across generated evidence, changeset, remote replication and restored snapshot.
10. Explicit test shows physical output is blocked locally despite broker crash, forged approval, expired credentials and site network partition.

## 16. Implementation milestones

- **SW0 — Architecture freeze:** approve node identities, schema registry, frame-v1 fixed header, queue classes, trust boundaries; golden frame examples.
- **SW1 — Core binary MCU library:** `ce_core`, `ce_wire`, `ce_queue`; zero-allocation parser, fixtures, static linker size report, interrupt-driven loopback.
- **SW2 — Local durable host:** `ce_store` and single-process `ce_mesh` backed by journal, snapshots, idempotent digital event callbacks; translate Python v1 envelopes to/from binary without losing semantics.
- **SW3 — Secure join and networking:** reviewed crypto adapters, OS event loops, QUIC host sessions, authenticated embedded gateway, capability ACLs, subscriptions, push receipts.
- **SW4 — WindChange:** candidate -> evidence -> validate -> durable commit -> announce -> fetch -> stage -> apply; causal DAG and deterministic snapshot restore.
- **SW5 — Acquisition integration:** vendor-approved read-only adapters, clock domains, chunked streams, backpressure and loss accounting; independent A22 limit boundary remains untouched.
- **SW6 — Multi-host and data center:** replicated object store, partition/reconnect convergence, admission control, observability, fault injection, benchmark reports and security review.

**Exit gate:** No phase may claim reliability, realtime behavior, evidence integrity or biological capability from documentation or mocked data alone. Publish measurements, commit hashes and tests for each profile. All living-system-facing execution remains under independently approved device and laboratory boundaries.

## 17. Architecture decision register (proposed)

| ADR | Decision | Rationale | Revisit if |
|---|---|---|---|
| ADR-SW-001 | shared C++20 core, platform adapters | portable ABI-free code and bounded MCU path | silicon/compiler makes C++20 impractical |
| ADR-SW-002 | WindPacket/1 fixed header + TLVs | compact deterministic decode and evolution | measurements show prohibitive overhead |
| ADR-SW-003 | QUIC/TLS 1.3 between hosts | secure multiplexed reliable streams + optional datagrams | measured performance/deployment incompatibility |
| ADR-SW-004 | authenticated secure serial/CAN for MCU | constrained interrupt-driven edge links | platform secure transport is better vetted |
| ADR-SW-005 | immutable changesets over CAS + event journal | reproducibility, deduplication and audit | legal retention needs alternate policy |
| ADR-SW-006 | per-endpoint weighted queues + credit | bounded latency and overload containment | unfairness/starvation benchmarks fail |
| ADR-SW-007 | local-first event commits and reconnection sync | disconnection tolerance without polling | consistency requirements change |
| ADR-SW-008 | independent physical limit path | fault containment and reproducible experiments | do not weaken this separation |

## 18. References (engineering standards and repository anchors)

- RFC 9000, QUIC transport: https://www.rfc-editor.org/rfc/rfc9000.html
- RFC 9001, TLS 1.3 security for QUIC: https://www.rfc-editor.org/rfc/rfc9001.html
- RFC 9221, QUIC DATAGRAM (explicitly unreliable): https://www.rfc-editor.org/rfc/rfc9221.html
- Noise Protocol Framework, handshake properties and identity caveats: https://noiseprotocol.org/noise.html
- NIST SP 800-57 Part 1 Rev. 5, key management: https://doi.org/10.6028/NIST.SP.800-57pt1r5
- Repository: https://github.com/bradgearon/ChristEye-Knowledge

**Open design questions for SW0:** hardware transport(s) actually selected for M0; endpoint memory/flash budgets; acceptable clock sync uncertainty; storage retention and consent jurisdiction; digital rollout/approval roles; hosted vs on-prem cluster boundary; minimum rate/latency SLO by instrument. These are engineering parameters requiring measurement rather than invented guarantees.