# Canonical evidence storage and legacy migration

The compiled `anaiero_evidence` and `anaiero_storage_native` targets implement
the common runtime's canonical commit path. Their public contracts are
`anaiero/sdk/evidence.hpp` and `anaiero/sdk/storage_tools.hpp`. Existing legacy
Registry/controller formats and commands remain available on their old paths.

## Commit and recovery contract

One `CommitBatch` can carry configuration/state records, checkpoint/artifact
blobs, observations, receipts, generic registry occurrences, source lifecycle
records and expiry decisions. The store serializes their admission under one
native writer lock. It verifies record revision/digest guards before effects.
The committed transaction retains the original guards as well as its effects.

The durability order is:

1. Write a bounded staging intent and flush new content-addressed blobs.
2. Write and flush the complete immutable transaction, linked to its predecessor.
3. Atomically publish and flush `ROOT`. This is the canonical commit boundary.
4. Advance the derived index and its coverage marker; reclaim obsolete index
   nodes; remove the staging intent.

A failure before publication leaves the previous root authoritative. Restart
removes the staged uncommitted blobs and transaction. A failure after publication
returns `committed_degraded`, including the original canonical receipt, and
stops admission until index recovery. Index failure cannot change a committed
operation into an uncommitted one. A directory-flush failure can leave the
caller uncertain whether publication became durable: stop admission, reopen or
repair, then reconcile the stable operation ID before retrying effects.

An operation ID, original request digest and semantic batch digest identify a
retry. An exact committed retry returns the original receipt. Changed effects
or changed authorization claims conflict. Admission preconditions are retained
in the original transaction but excluded from the semantic retry digest: a
retry can observe a later generation without creating a second effect.

Records have monotonic revisions. Tombstones retain their revision, so removing
and recreating a key cannot satisfy an old “never existed” guard. Strict
observations enforce contiguous `(source UID, epoch, sequence)` identity;
conflicting evidence at an existing sequence is rejected. Generic Registry
events retain the older occurrence semantics: different objects/origins may
share a tuple, exact duplicates are suppressed, and a duplicate does not move
the latest-occurrence pointer backward.

`AuthorizationGuard` adds bounded principal, scope, credential generation and
expiry claims. The native adapter samples its trusted admission clock once,
immediately before staging writes. Each nonzero deadline must be strictly
later than that sample. The receipt retains the sample and authorization
claims. Expiry while waiting in the queue rejects the uncommitted request;
expiry after admission does not revoke a committed result. Policy revision
and retirement authority still require service-supplied `RecordGuard` fences.
The storage adapter does not invent a domain authorization policy.

The default clock is native UTC. `IAdmissionClock` permits deterministic tests
and trusted host integration; the host must keep that object alive for the
store's lifetime. A clock sample earlier than the last durable admission time
fails closed. `StoreInfo.admitted_unix_s` exposes that persisted floor to
read-only outbound authorization, and compaction preserves it in its base.
Time corrections and clock trust remain deployment concerns.

## Indexed history and bounded ownership

The initial native backend is a sealed, immutable AVL index, rather than a
required SQLite installation. It works with the existing Windows/Linux C++20
toolchain and requires no Python, database server, network download or new
third-party runtime. `IEvidenceIndex` exposes typed indexed reads separately
from canonical mutation and blob I/O. SQLite remains a possible future backend,
not an implemented or required dependency of this increment.

The derived coverage file is `INDEX-COVERAGE`, beside the `index/` node
directory. These names remain distinct on case-insensitive filesystems.
Existing case-sensitive stores with the former `INDEX` file are compatible:
when the new marker is absent, opening validates the old marker against the
canonical transaction and index, durably publishes `INDEX-COVERAGE`, then
removes the validated old marker. Canonical roots, transactions and payloads
are unchanged. If both files exist, the new marker takes precedence; corruption
does not silently fall back to the old marker. Invalid old files are retained
during an explicit rebuild. A nonregular `INDEX-COVERAGE` path blocks repair
until the operator resolves that path conflict; it is never overwritten. The
old spelling is ignored as a directory only when it is the filesystem alias of
`index/`.

Index keys cover operation receipts, current records, retained record revisions,
strict observation identities/source tips/expiry floors, generic exact/latest occurrences,
blob ordinals and reference counts. Record entries contain bounded canonical
generation/slot/digest or canonical-base-page/slot/digest locators; large record payloads are not copied into tree
nodes on every tree rotation. The sealed root aggregates index byte size and
maximum node size for admission accounting. Bounded index-update and cleanup
manifests prevent a large reference graph from bypassing the batch limit.

Derived index node writes and obsolete-node deletion use batch directory
flushes. At most 8,192 verified nodes and 32 MiB of node data are cached during
one serialized admission/update, including every newly staged path-copy
version. New nodes that fit this bound stay in memory until publication;
versions superseded within the same transaction never reach disk. A full cache
uses the original bounded spill-to-file behavior. Before publishing coverage,
all reachable staged nodes are written and the index directory is flushed.
A separate bounded 8 MiB/8,192-key GC memo avoids repeating a final-tree search
for every intermediate version of the same key. Cleanup removes only obsolete
on-disk nodes after coverage publication; shared current nodes remain intact.
The cache is discarded after each operation, including rejected admission.
Statistics expose actual writes, staged/spilled versions, transient versions
elided and peak node-cache counts/bytes. Canonical blobs, transactions, `ROOT`,
index coverage and cleanup intents retain their durable publication protocol. Node file contents
are a reconstructible cache: a power cut can require index repair even when
the canonical commit was acknowledged. A missing node still fails closed and
does not become an empty query result.

Healthy startup reads the canonical root, the covered transaction and index
root, plus any bounded interrupted-work intent. It does not enumerate all
historical records. Point queries take logarithmic index-node work. Range pages
take a seek plus output work, with bounded canonical transaction reads for
record payloads. Statistics expose actual node reads/writes and point/range
queries. These are algorithmic work counters, not latency or hardware guarantees.

Every traversed node must match its content digest. Missing or corrupt nodes
fail closed; they are not interpreted as missing evidence. Coverage markers
bind an index generation to a canonical transaction hash. A damaged index may
open in `index_ready=false, writable=false` mode so an operator can call
`repair_index()`. Repair verifies canonical history, then rebuilds or resumes
the derived index. It cannot repair corrupt canonical evidence. Rebuild is an
explicit historical operation and needs temporary disk headroom.

Full audit verifies the canonical chain, admission-time order, every retained
blob, index structure, coverage counts, canonical witnesses and live reference
count aggregates. Ordered history/current-directory merges verify latest
record revisions, contiguous source sequences and generic latest occurrences.
It uses bounded pages and group accumulators, including retired histories;
there is no full history scan for each observation. Expired blobs are reported
as declared unavailable, not falsely reported as payload bytes reverified.

`CommitWorker` is a separate native storage owner. Accepted requests reserve
both request and completion capacity and obey a byte bound. Completions remain
available until drained. `cached_info()` reads a cached snapshot even while
storage is stalled. The optional wakeup callback is a signal on the storage
thread; domain completion/projection belongs on the consumer. Accepted work
is drained during stop. Filesystem calls are not universally cancellable and
the worker does not claim a hard shutdown deadline during a host I/O stall.

## Retention, quotas and explicit limits

Current non-tombstone `Record.references` are protected roots. References from
unexpired blobs protect children. New blob edges refer to existing or earlier
blobs in the same batch, so creation has an explicit bounded dependency order.
Historical observation/occurrence metadata is not an immortal payload pin.

Expiry is canonical before physical deletion. A batch may remove a current
root and expire its now-unprotected graph atomically. A protected blob cannot
expire. `get_blob()` returns `history_expired` as soon as expiry commits, even
if physical bytes are still present. Reclamation requires the expected current
generation, removes the expired file and commits accounting. An interruption
between removal and accounting is safe to retry. Expired identities cannot be
silently resurrected by submitting the same content again.

Admission reserves bounded canonical/index working space and checks filesystem
free space. `StoreInfo.physical_bytes` accounts canonical transaction files,
canonical base manifests/pages, present blob payloads, reachable index-node bytes
and fixed metadata allowance. Pending compaction cleanup adds a conservative
upper bound for obsolete files until their durable removal completes;
it is not an OS allocation-unit/inode quota. The filesystem free-space check
and conservative copy-on-write reservation complement that accounting.

Ordinary commits additionally leave a finite maintenance reserve: by default
16 MiB, capped at one eighth of the configured absolute byte budget.
`CommitClass::maintenance` allows reviewed expiry, release, retirement and
recovery metadata to use that reserve. The receipt records the class. It does
not bypass the absolute byte budget. Physical reclamation can credit bytes
already removed under a committed expiry, but its accounting transaction must
also fit. Reserve exhaustion is an explicit `capacity` outcome; an operator
must review capacity or storage policy rather than relying on an unlimited
emergency exception. Hosts must persist and reapply their selected
`StoreOptions`; these are host admission limits, not silently inferred policy.

## Reviewed canonical history compaction

`preview_native_compaction`, `execute_native_compaction` and
`resume_native_compaction` are native maintenance operations, serialized with
commits and copied indexed reads. A preview pins the exact canonical generation,
terminal digest, existing base, derived index and operator policy. Execution
recomputes that plan and audits its canonical authority before it writes a
replacement. A changed commit, pin or lifecycle record invalidates the plan.
Temporary work must fit both the explicitly declared working budget and the
store's absolute quota, with a separate native free-space check. Maintenance
cannot assume that its future reclaimed bytes are already free.

The publication sequence is:

1. Publish a durable maintenance intent; retain the complete previous root.
2. Stream bounded canonical snapshot pages and construct a replacement index.
   Verify snapshot digests, sorted coverage and index structure before activation.
3. Publish `ANAIERO-CANONICAL-ROOT/2`, naming the sealed canonical base manifest.
   The original terminal transition digest and generation remain unchanged.
4. Publish derived coverage bound to that base, then remove the old transaction
   prefix, obsolete index generation and unreferenced older base pages.
5. Remove the cleanup intent only after deletion and directory flushes finish.

The manifest records the exact original terminal anchor, cumulative canonical
byte position and admission-time floor; operator/plan identity; retained row
counts; summarized revision and observation counts; previous base digest; and
ordered page identities/ranges. Pages hold at most 128 rows and are assembled
at an 8 MiB target, with a 32 MiB hard decoder bound for one large record.
The manifest is bounded to 100,000 page descriptors and the 64 MiB canonical
decoder. Working state consists of fixed pages, group accumulators and the
bounded index cache; the manifest's page directory is retained for indexed
canonical lookups. Exceeding these bounds fails explicitly.

Default compaction preserves every latest record, including source ownership,
retired identities, policy and tombstones; every blob identity and expiry fence;
every exact operation receipt; source tips; and generic occurrence identities.
It summarizes overwritten record revisions. It removes only a *contiguous*
observation prefix whose payload **and** receipt blobs have canonically expired,
retaining the final original claim as an internal boundary witness. A distinct
per-stream floor makes queries or new claims at/below that boundary return
`history_expired`. A retained later observation or an expiry gap cannot be
silently skipped. No bootstrap, receipt or delivery acknowledgement is invented.

Interrupted staging leaves the old root authoritative. Interruption after new
root publication returns `committed_degraded`: the canonical base is committed,
but cleanup/index recovery remains pending. Reopen or `resume_native_compaction`
reconciles the published root, rebuilds a lost index from verified base plus
retained suffix, and finishes permitted deletions. Before cleanup is complete,
its obsolete bytes continue to count against capacity. A retry of the same
reviewed plan resolves its committed manifest rather than publishing twice.
Records are returned as copied bytes; no reader mapping is deleted underneath
an external lease.

Audit reports `summarized_generations`, `summarized_record_versions`,
`expired_observations`, base digest and page count separately from retained
suffix generations. It verifies present base/suffix bytes and committed summary
anchors. Original prefix bytes that were removed cannot be reverified.
A damaged base or retained suffix is corruption; index repair cannot invent
removed originals. Older binaries reject ROOT/2, so a preserved backup/export
is required for an old reader. Ordinary append transaction formats and their
original identities remain unchanged.

### Explicit receipt-history epochs

Default compaction retains exact retries. `expire_operation_receipts=true` is a
separate, explicit operator decision that discards those receipts and advances
`StoreInfo.operation_epoch`. After that boundary, an unknown unqualified or
older-epoch operation returns `history_expired`, both on lookup and admission.
It never becomes a safe novel operation merely because its receipt disappeared.
`qualify_operation_id(epoch, logical_id)` constructs the reserved
`__epoch/<16-lowercase-hex>/<logical-id>` identity for a genuinely **new** operation
in the declared current epoch. Unknown future epochs are rejected. Qualified
current-epoch IDs retain ordinary exact retry/conflict behavior.

Never relabel an uncertain old operation with the new epoch. Its old result is
unavailable and must be reconciled through its durable effects or explicit
application recovery. A running application must establish a new persisted
intent/identity boundary before it uses a fresh namespace. The modular CLI
therefore refuses receipt expiry on existing session stores; ordinary default
compaction remains compatible with their durable cycle/experiment identities.
Read-only `inspect_native_operation_epoch` inspects a quiescent format without
opening or mutating its writer/index. Native restore currently rejects a source
with an explicitly rotated epoch before copying: its quarantine operation has
not established a new application identity boundary. Default-compacted stores
retain full native restore support.

Internal expiry reclamation derives a current-epoch maintenance operation only
after confirming the canonical object remains pending physical reclamation.

Compaction bounds repeated state history and allows an explicitly retired
operation namespace to release receipt metadata. It does **not** erase arbitrary
latest source/catalog identities, blob resurrection fences or generic exact
occurrence identity. Their cardinality still consumes finite quota. Payload
reclamation and prefix removal are real, but finite storage still requires
reviewed retention/identity policy, archive capacity or stopped admission.
Do not delete canonical transactions, base pages or index files manually to
manufacture space.

The OS adapter flushes files and uses atomic replacement; Windows uses
write-through rename. Actual power-loss behavior remains dependent on the
filesystem, device and platform. Deterministic crash cuts, quota failures and
corruption tests are implemented; long-duration soak, actual disk exhaustion,
power-loss qualification and Windows execution need their declared release
profiles. Passing a Linux unit suite does not substitute for those results.

## Evidence-only migration

Run with the old writer stopped:

```text
ce_modular_session migrate-evidence LEGACY_STORE NEW_STORE
ce_modular_session audit NEW_STORE
```

The native adapter acquires the existing legacy `writer.lock` and a separate
destination migration lock. It fingerprints and copies the source without
opening a `Registry` on the original. It retains one byte-exact raw snapshot
and opens a second private recovery clone, because legacy opening may truncate
uncommitted journal tails. The recovered clone undergoes the legacy full audit.

Migration traverses `visit_events`, including archived segmented history,
rather than the bounded resident tail. It imports every verified CAS object,
including opaque objects without a current event, and pins them individually.
Each `legacy.event` record preserves the original committed event wire bytes,
their wire digest and the original legacy event hash. Generic tuple/object/
origin semantics are preserved. Temporary uncommitted CAS files remain in the
raw snapshot and are not promoted into committed evidence.

Stable per-object/per-event operation IDs permit retrying partial imports.
The destination remains unpublished until the complete new canonical store
passes audit. Activation uses an atomic directory rename that refuses an
existing destination. `NEW_STORE.legacy-source` retains the raw source snapshot;
its files are marked read-only. The original source is never upgraded in place.
The report names this backup and counts its bytes separately from the target
quota. Migration reserves capacity for the target quota plus two source copies
and conservative per-file overhead. It currently accepts at most 65,536 files,
1,024 bytes per relative path, and 64 MiB per source file; source file bytes
must also fit the configured target byte budget.

An interrupted import retains `NEW_STORE.migration` for a retry with the same
unchanged source. A completed retry verifies the destination and retained
snapshot. A changed source requires another destination. Evidence migration
does not claim to transform a legacy SW5/SIM3/numerical live-session checkpoint
into a new common-runtime engine state. Keep the raw snapshot and original
engine/module artifacts for that separate recovery/migration decision.

## Native verification

### Exported artifact publication

The native `write_new_artifact` adapter uses a per-target writer lock and a
small sealed intent containing the normalized target, content digest and byte
count. The intent is flushed before staging begins. A retry can adopt complete
staging or rebuild an exact partial prefix when the persisted intent establishes
ownership. Unowned staging, different ownership and mismatched bytes are
preserved. An atomic no-replace rename publishes the complete file. Retries
that find an identical published file verify bounded bytes and flush the file
and its directory again, covering publication-before-directory-sync cuts.

Artifacts are limited to 8 MiB. Existing files are read at their checked size
with a trailing-byte check, so growth cannot defeat that bound. Target locks
remain as coordination metadata; owned completed intents/staging are removed.
Windows case aliases and aliases of the parent directory share target
coordination. This does not grant permission to overwrite another artifact.

### Test commands

```text
cmake -S native -B build-evidence -DCMAKE_BUILD_TYPE=Release -DANAIERO_CONTROLLER_ONLY=ON -DCE_WIND_BUILD_TESTS=ON
cmake --build build-evidence --config Release --target anaiero_evidence_tests anaiero_migration_tests anaiero_artifact_tests
ctest --test-dir build-evidence -C Release -R "^anaiero_((evidence|migration)_tests|artifact_publication)$" --output-on-failure
```

The suites cover whole-root state/evidence atomicity, five durability cuts,
operation retries/conflicts, policy revocation fences, strict/generic history,
protected graph expiry/reclamation, distinct index marker paths, legacy marker
adoption and preservation, index root and child corruption, rebuild,
indexed point/range work, quota rejection, single-writer locking, artifact
tampering, queue/completion capacity, queued authorization expiry, admission
time recording/rollback, finite maintenance headroom, legacy quiescence, exact wire preservation, archived
history, opaque provenance pins and resumable unpublished migration failure.
The artifact suite separately checks durable retries, complete/partial staging
recovery, lock exclusion, path aliases, oversized inputs, and preservation of
conflicting destinations, intents and staging files.
