---
title: "CE-SW-40 — Operating Envelope, Receiver Retention and Package Lifecycle"
status: "Native implementation and reproducible qualification; hosted latency and prolonged release evidence remain open"
---

# CE-SW-40 — Operating envelope, receiver retention and package lifecycle

This continuation advances the operational closure work identified in
[CE-SW-39](CE-SW-39_continuous_native_operation.md). It preserves the existing
M1–M6 sequence in [CE-SW-37](CE-SW-37_modular_runtime_and_evidence_lifecycle.md).

| Delivery | Implemented result | Evidence boundary |
|---|---|---|
| Reduce canonical index work | Defer checks for transient index nodes until publication and coalesce repeated source-tip replacements within a transaction | Same canonical format, budgets and durability order; measured reports determine the operating envelope |
| Qualify the mixed continuous host | Native five-engine profile with bounded control/storage pressure, durable retry, historical probes, orderly restart and full audit | Short completed runs are functional operating evidence; elapsed 24-hour/seven-day profiles remain separate gates |
| Reclaim receiver history | Reviewed, fenced expiry of received observations, delivery receipts, artifacts and superseded bootstrap checkpoints, followed by resumable deletion | Protected roots and immutable authority/retry identities remain retained; finite metadata growth still needs explicit capacity policy |
| Select and roll back packages | Exact installed inventory, native executable probe, immutable review artifact, canonical activation and suffix-preserving rollback | Same compiled engine identity; stopped supervisor switch; no semantic engine migration or automatic live upgrade |

## 1. Preserve measurements and reduce avoidable work

The [hosted Windows 100,000-observation report](../../qualification/reports/2026-11-18-hosted-windows-100k/README.md)
for PR #53 head `c196e56` completed with correct recovery, full audit, historical
lookup, memory and disk results, but failed its unchanged commit-p99 histogram
upper-bound budget: 2.147483648 seconds against 2 seconds. The report, exact
profile and original CI artifact remain linked. The earlier
[shared-sandbox failure](../../qualification/reports/2026-11-18-shared-sandbox-100k/README.md)
also remains evidence. A later improvement does not rewrite either outcome.

The derived AVL index now checks an existing content-addressed destination
when a staged node survives to publication. Temporary path-copy nodes that
will be discarded no longer issue filesystem existence probes. The existing
8,192-node/32 MiB staging bound remains; overflow uses the verified spill path.
Existing destination bytes still have to match exactly, and every reachable
node is published before index coverage advances.

Each observation keeps its own canonical claim and index entry. Repeated updates
to a source's tip within one transaction are coalesced into its final tip. The
temporary map is bounded by the admitted observation count. Canonical blobs,
transaction, ROOT and derived-index publication retain their existing ordering.
Repair can produce a different derived tree layout while preserving canonical
root, generation, operation receipts and observation identities.

Regression coverage includes pre-existing node corruption, bounded cache spills,
multiple source tips, interrupted publication, index repair and old-store
compatibility. Performance acceptance still uses the predeclared profiles; a
small diagnostic comparison is not a substitute for the Windows/Linux report.

The [retained operating-envelope measurements](../../qualification/reports/2026-11-18-operating-envelope/README.md)
keep both passing and failing hosted runs, original CI archives, exact profiles,
actual checkout identities and source/build manifests. Qualification now records
the explicit `log2-ceiling-capped-by-observed-maximum/1` percentile method: the
smaller of the selected logarithmic ceiling and the exact observed maximum is
still an upper bound on the same nearest-rank observation. A sorted-sample native
oracle verifies that guarantee, including a real over-budget p99 and an outlier
outside p99. No profile limits changed and no historical report was reclassified.
The tighter estimator is separate from the storage optimization and must not be
interpreted as a runtime speedup. Windows raw CRLF manifests and their explicitly
derived LF build-identity witnesses are retained separately.

At runtime `aec1489d`, the PR-triggered 100k profiles passed on both platforms.
A separate Linux push run failed the same unchanged p99 upper-bound gate, with
2.147483648 seconds against 2 seconds and an exact maximum of 3.543940010 seconds.
All its correctness, recovery, audit, memory and disk gates passed. The histogram
does not establish the exact p99 inside the selected bucket, and the cause of
the runner variation is not proven. The operating-envelope record therefore
keeps hosted latency qualification unresolved. The next measurements need exact
threshold counts, durability-stage timing and a declared storage-device profile.

## 2. Mixed-engine continuous-host qualification

The native qualification tool now has a separate mixed operation:

```text
anaiero qualify mixed qualification/native/mixed-ci.cevqualification examples/runtime/modular-native.cevsession NEW_OUTPUT
ce_native_qualification mixed qualification/native/mixed-ci.cevqualification examples/runtime/modular-native.cevsession NEW_OUTPUT
```

It admits the actual retained session definition and runs Cell Behavior (SW5),
Multi-Colony (SIM3), the finite VLM plan, passive voltage and scalar environment
through `NativeSession` and `NativeContinuousHost`. Each configuration, input,
source identity and final state is reported. The finite VLM finishes according
to its program; it is not artificially kept running to fill a duration budget.

The workload pauses and resumes the environment engine with an exact repeated
operation ID, fills bounded storage/control queues behind a deliberately blocked
storage task, verifies quota rejection in a separate small store, and reads
historical payload/receipt pairs through the serialized lane. It stops at
declared cycle checkpoints and reopens the actual session, checking the exact
store root, cycle count and all engine states before continuing. A final full
audit checks retained canonical evidence.

The report separates cached snapshot latency, durable engine-control latency,
queued historical-probe latency, healthy reopen, audit, wall time, CPU, resident
memory, queue counters and sampled session-store allocation. These are distinct
from per-cycle dispatch/commit/projection distributions and remote ACK latency,
which this harness does not measure. The pressure event is an injected bounded
storage-lane delay and declared quota rejection; it does not simulate a full
physical disk, TLS fan-out, an OS crash or a power failure.

Exact profile bytes, original session text, canonical session definition and a
machine-readable report are written only to a new output directory. Runtime
failures retain their report when the process can publish it. Invalid admission
or an existing destination is rejected before replacing any evidence.

The `mixed-24h` and `mixed-7d` profiles retain explicit elapsed minimum durations
and finite numerical horizons. Pacing changes wall time rather than engine-clock
semantics. Their existence does not mark those gates passed: a completed report
on the declared platform/device is required. See the
[qualification guide](../../qualification/native/README.md) for the exact schema,
budgets and workload boundaries.

## 3. Reviewed receiver retention

Sender lifecycle retention and receiver provenance are separate scopes. Receiver
retention operates on the authenticated relay's namespace without inventing local
engine observations. `ReceiverRetentionRequest` selects one principal/source/epoch,
a bounded observation interval, and optional exact artifact, bootstrap-offer or
delivery-receipt identities.

The three publication stages are:

1. **Review.** Verify selected content and its original receive witnesses. Commit
   an immutable plan with the store identity, reviewed root/generation, exact
   candidate digest, object list and logical byte total.
2. **Activate.** Reject any intervening store mutation, recompute the candidates,
   and commit reference removal, permanent history tombstones, blob expiry and
   the activated plan together. Native reference-count admission protects every
   remaining cursor, projection, pin and other root.
3. **Reclaim.** Delete only canonically expired objects and durably reconcile
   their byte accounting. An interrupted cleanup resumes with the same plan and
   operation. Completion does not remove authority or retry identities.

An observation interval contains at most 32 observations. Every still-retained
member of an original delivery page must be selected; already-tombstoned members
outside the interval are accepted as evidence of earlier explicit expiry. This
lets overlapping subscriptions with different page boundaries be reclaimed in
successive bounded operations. Up to eight explicit receipts, artifacts and
bootstrap IDs each can be selected, with at most 64 distinct objects per plan.

Overlapping subscriptions may retain independent full-page receipts. Select
additional copies by the returned `receipt_sha256` using `receipt:SHA`; this is
bounded explicit cleanup, not a hidden scan over every historical subscriber.
Current receive cursors and latest projected values remain protected. A bootstrap
checkpoint becomes eligible only after the receive cursor no longer depends on
it. Unknown, differently scoped, partially selected or changed evidence is not
interpreted as available deletion space.

Expired observation reads and exact old delivery/artifact/bootstrap retries
return `HISTORY_EXPIRED`. Accepted operation and source-owner identities remain
canonical, preventing old data from being silently reintroduced as a new delivery.
Default canonical compaction may subsequently summarize redundant journal
versions; it does not remove protected identities or enable a new operation epoch.

The same missing-file accounting repair also applies to existing sender lifecycle
maintenance: physical absence after an interrupted delete is insufficient to
claim completion while canonical quota accounting is still pending.

See the [mesh operator guide](../../native/docs/continuous-mesh.md) for commands,
quiescence requirements, retry semantics and finite retained-metadata limits.

## 4. Native package selection and rollback

The native deployment adapter verifies every installed file against its sorted
size/SHA-256 inventory, rejects extra/non-regular/symlink entries and launches a
bounded host probe. Package directories are immutable, administrator-owned inputs;
this is local integrity and compatibility admission, not publisher authentication
or protection against an administrator concurrently replacing executables.

An immutable review artifact binds the exact candidate package and previous
selection to the current store ID, root, generation and selection revision.
Activation takes the native writer lock, verifies the current retained recovery
witness and target installation, checks all scheduled engine identities, and
commits the new selection, retained host/manifest bytes and operation receipt
atomically. A pending durable cycle must be reconciled before deployment.

After activation, stop/switch/start is an explicit supervisor operation. A host
that implements this selection protocol rejects a mismatched selected executable
path or modified selected installation before serving the session. Older hosts
without that protocol are outside this deployment workflow and must not be used
as a way around its selection check.

Rollback reviews the previous compatible package against the **current** store
root. It preserves observations and accepted operations written since the earlier
activation. A damaged current installation does not block recovery to an intact
previous package when its protected canonical recovery witness is valid. Repeating
an old activation returns its original receipt and never silently reselects it
after a later deployment.

This batch supports unchanged compiled engine identity and canonical formats.
Different engine implementations require an explicit state migration and remain
rejected. Retained host/manifest bytes are recovery witnesses; operating-system
libraries, OpenSSL and complete external installation directories still require
the declared deployment dependencies. See
[package lifecycle](../../deployment/native/package-lifecycle.md) for the command
sequence, compatibility matrix and tested failure boundaries.

## 5. Remaining release gates

These changes improve the implemented operating paths. They do not raise the
engineering assessment to a production-release claim. Remaining evidence includes
the declared million-observation run, completed 24-hour/seven-day mixed profiles,
continuous TLS fan-out with lag/churn/rotation, actual device-pressure and
crashed-host/power-loss experiments, independent installed-SDK adoption and
supported semantic engine migrations. Bootstrap still requires producer quiescence
through acceptance. Permanently retained authority/identity fences still consume
finite storage.

The [development plan](native_development_plan.md) orders this remaining work.
Exact commit/run reports accompany each implementation PR; architecture text and
short regression counts do not replace those operating measurements.
