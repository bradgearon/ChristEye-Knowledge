# CE-SW-33 — Sustained native storage and recovery

This increment adds opt-in segmented storage for native Anaiero sessions. The
existing `CEWREG/1` bounded store remains supported. A sustained session can
commit more than 4,096 events without retaining its lifetime journal in memory.
Windows and Linux use the same record formats and commit protocol through the
existing native file adapter.

The implementation retains historical evidence. It bounds the resident journal,
latest-value index, source projection and active recovery work. It does **not**
promise constant total disk consumption: immutable archives grow with retained
evidence, and admission fails when configured free-space headroom is unavailable.

## Components and ownership

The public `christeye::living::durable::Registry` facade selects the legacy store
or `SegmentedStore`. `durable_records.hpp` contains shared record encodings and
bounded file reading. Native file operations remain in `durable_io.hpp`.

`ObservationCheckpointService` owns a second, bounded projection on the journal
worker. This mirror advances only after durable event publication. It preserves
both `ColonyState` and the exact last observation for every source/epoch pair.
`WindStoreEpochObservationJournal` encodes that state and binds it to a global
journal sequence and event hash. The live projection still advances from durable
completion receipts on its own owner; storage never calls it from the worker.

`AsyncEpochJournalWorker` caches a plain-data capacity snapshot. Producer queries
and admission hold the queue mutex; Registry access stays on the worker. The
initial snapshot is read before the worker starts. Queued and active writes
reserve conservative append bytes and event slots. Every accepted operation
also reserves its completion slot, including unsuccessful completions.

Session metadata has a separate Registry and owner. `WindStoreRecordStore`
loads the latest manifest through the persistent index. It records a metadata
checkpoint after one configured segment's additional global metadata events.
The marker contains the manifest bytes and an explicit reference to its CAS
object. The persisted session record also pins engine behavior identifiers,
schemas, clock domains, summary meanings and supported capabilities; records
with declared contracts cannot silently change them on continuation.

## Opt-in and API compatibility

```cpp
auto options = durable::StorageOptions::sustained(256);
auto status = registry.open(directory, options);
```

`open(directory)` preserves legacy behavior for an ordinary old store. It detects
an existing `MANIFEST` and reopens that segmented format automatically. Explicit
sustained open imports a valid legacy journal under the writer lock. Migration
validates the committed legacy history first, prepares segments and indexes,
and publishes `MANIFEST` only when the complete import is ready. The old journal
and HEAD remain available. Imported global event hashes and reader cursors are
preserved, including repeated source identities in valid legacy logs.

Legacy objects can contain references opaque to the storage layer. Migration
therefore creates conservative `legacy-import-*` pins for existing CAS objects.
They must remain until an application can verify replacement reference roots;
storage does not guess that an old checkpoint or artifact is expendable.

The existing `put`, `get`, `publish`, subscription and cursor methods remain.
For a segmented Registry:

| API | Meaning |
| --- | --- |
| `event_count()` | Total committed global event count |
| `events()` | Only the bounded active segment; never lifetime history |
| `latest_event(kind, entity, clock, out, found, producer)` | Latest committed event for a stream, using one bounded index bucket |
| `find_event(kind, entity, clock, producer, tick, out, found)` | Source-qualified identity lookup, including historical retries |
| `visit_events(after, visitor)` | Stream committed history after a global cursor, one segment at a time |
| `capacity()` / `can_append(count, bytes)` | Current bounds and filesystem headroom |
| `reserve` / `release` | Bounded owner-side capacity reservations |
| `put_referenced(bytes, child_hashes)` | A CAS object with declared immutable child references |
| `save_replay_checkpoint` / `load_replay_checkpoint` | A checkpoint bound to the committed global sequence and hash |
| `pin(name, roots)` / `unpin(name)` | Explicit roots for active work or external owners |
| `archive_ready()` | Verify and archive eligible closed segments |
| `collect_objects(stats)` | Audit retained evidence, archive historical objects and remove proven orphans |

A generic stream need not have monotonic source ticks. Index entries retain
both the latest event and the maximum source tick. New monotonic events use
bounded index lookup; an older retry can require historical streaming. These
operations use bounded memory, but historical lookup time grows with the
history examined. The route-receipt and checkpoint-catalog adapters use the
new lookup/iteration APIs so archived history remains visible.

## Files and atomic publication

A segmented store contains:

| Path | Purpose |
| --- | --- |
| `MANIFEST` | Sealed `CEWSTORE/2` root: limits, active segment, committed offset, global head, archive watermark, checkpoint cursor and 64 index hashes |
| `segments/<20-digit-id>.log` | Active or retained hot segment, with `CEWSEG/1` header and globally chained events |
| `segments/<id>.meta` | Closed-segment range, byte count, content hash and preceding segment-metadata hash |
| `archive/<id>.log`, `.meta` | Verified immutable cold copies selected by the committed archive watermark |
| `indexes/<sha256>` | Private immutable latest-value index buckets |
| `objects/<sha256>` | Hot application CAS objects |
| `archive_objects/<sha256>` | Historical CAS payloads moved out of the hot set |
| `references/<sha256>.refs` | Sealed explicit child references |
| `cursors/<reader>.cursor` | Durable reader acknowledgement and event hash |
| `pins/<owner>.pin` | Explicit object roots that must survive collection |
| `gc_marks/` | Disposable derived disk marks, rebuilt before collection |

Each append prepares its immutable index bucket, appends and flushes the event,
and atomically publishes the new manifest. Readers never treat appended bytes
beyond the manifest offset as committed. Recovery truncates only that
uncommitted tail. The old index generation remains available until the new root
is durable. Unreferenced private index generations are pruned after publication
or on restart; they are not application CAS objects or public pin targets.

Rotation first writes the closed-segment descriptor and the next segment
header. It then publishes a new manifest referring to the empty new segment.
The next event receives the next global sequence; neither rotation nor a
checkpoint creates a synthetic observation event.

The implementation injects interruptions after segment preparation, rotation
publication, journal flush, temporary-root flush and root publication. Tests
reopen each interrupted store, verify the precise committed prefix and retry
without losing or duplicating a committed event.

## Snapshot and tail recovery

An observation checkpoint records the full bounded projection, every source
cursor and the exact last observation for each source/epoch. Its canonical
encoding includes the session name. Decoding checks sizes, enum ranges,
canonical bytes and a content seal. Restore checks cursor uniqueness, source
clock identity, source/epoch sequence continuity, projection counts, metric
identity and enrolled permissions before accepting the saved state.

The Registry binds the checkpoint payload to the global head that was durable
when the checkpoint was saved. Loading verifies that cursor's event hash.
Recovery restores the checkpoint and replays only events after that cursor.
Source sequence checks continue from the saved per-source cursors. Readers
which do not support checkpoints can still request full streamed history.

Normal open verifies the small root, active segment, immediately preceding
segment anchor, current index buckets and checkpoint payload. It does not read
all cold history. `audit_disk()` explicitly verifies the full retained segment
chain, payloads, declared transitive references, latest-index events and
checkpoint provenance. Missing or corrupt old archives therefore fail full
audit and any replay needing those files, even when the independent active
recovery state can still be opened. This separation makes bounded recovery
possible without claiming that every cold byte was reread at startup.

Metadata recovery uses its persistent latest-manifest index, rather than
redecoding every previous manifest. Periodic metadata checkpoints advance the
archive watermark. A manifest's durable revision is returned to its caller
even when subsequent maintenance fails, so restart sees the correct committed
revision. `checkpoint_now()` supports explicit maintenance on the owner after
workers are stopped.

## Archive, readers and object reachability

A closed segment is eligible only through the minimum of the durable checkpoint
cursor and all registered reader acknowledgements. Slow readers retain their
unacknowledged hot segments. New readers may still replay already archived
history because the archive is retained.

Before publishing an archive watermark, storage verifies the segment and its
transitive object references, writes and flushes archive copies, and verifies
the copies again. Only after the manifest selects those copies can redundant
hot segment files be removed. An interruption before publication leaves the
hot representation authoritative. An interruption afterward can leave both
copies; retrying maintenance verifies the archive and finishes cleanup.

Object collection first audits retained evidence. Its live roots are current
latest-stream payloads, the recovery checkpoint, explicit pins and every event
in segments retained hot for recovery or readers. References are followed
transitively. A bounded in-memory live set protects these roots. Disposable
on-disk marks represent the complete historical reachability set without
loading lifetime history into memory.

A historical object outside the hot live set is copied and verified in
`archive_objects` before its hot copy is removed. An object is deleted entirely
only when it is neither live nor referenced by retained event history. This
preserves foreign active-operation pins, engine checkpoint payloads referenced
by manifests, slow-reader evidence and archived replay. Old recovery-marker
objects or abandoned writes can be reclaimed when no root references them.

Archived evidence itself is not automatically erased. Storage capacity remains
finite, and archival transfer to another medium is a separate future policy.

## Explicit bounds

| Resource | Bound or policy |
| --- | --- |
| Active segment | Default 256 events / 256 KiB; configurable 8–4,096 events and 8 KiB–4 MiB |
| Resident latest index | One bucket at a time, at most 256 stream entries |
| Persistent latest index | 64 buckets, each at most 256 entries; a full bucket reports capacity even if another bucket has room |
| Application object | At most 4 MiB |
| Child references per object | At most 512 supplied references |
| Recovery source identities | 32 `(source, epoch)` pairs |
| Projected metric entries | 256 |
| Durable archive reader cursors | 32 |
| Owner capacity reservations | 32 |
| Roots in one pin | 4,096 |
| Pin files read by collection | At most 4,096 |
| Live reference closure | At most 65,536 objects, at most 64 reference edges in a path |
| Default free-space headroom | 8 MiB, plus outstanding reservation bytes |
| Async evidence work | Default 32 outstanding operations, with a reserved receipt for each |

Limits fail closed; collection does not discard references to fit its working
set. Capacity reports are admission estimates, not an operating-system disk
quota: other processes can consume filesystem space, and a new stream can
still encounter its bucket limit. Every actual durable operation checks and
reports its result. Source retirement, index repartitioning and deletion of
old archive evidence are not implied by this increment.

The SIM3 runner has a separate existing format. It reads the bounded current
`culture.state` and its matching immutable tick snapshot, without lifetime
journal replay or a 4,096-event counter. It continues to preserve those history
files; this increment does not silently change its historical snapshot format.

## Verification and measurements

`native/tests/segmented_storage_tests.cpp` exercises sustained publication,
reader lag, reference accounting, pins, bounded latest indexes, global-cursor
fold recovery, exact projection recovery, legacy migration, metadata maintenance,
corrupt payloads/archives and interruption boundaries. It also checks that an
unsupported wide wire enum cannot narrow into the modeled-voltage metric.

One local strict C++20 `g++ -O1 -Wall -Wextra -Wconversion -Werror` run on Linux
committed **4,203 events across 132 segments**, with a **32-event** maximum
resident tail. Restart loaded **11 events**, and checkpoint plus those 11 tail
events produced the same fold as full replay. The initial full storage suite
completed in **40.47 seconds**, with **8,448 KiB peak child-process RSS** measured
by the local process runner. The 4,203-event case itself reported 37,006 ms.
The expanded suite, including metadata maintenance and the final migration/wire
checks, subsequently passed in 33.316 seconds with 8,576 KiB peak child-process
RSS. These are measurements of this fixture and workspace filesystem, not
throughput or memory guarantees for other hardware, object sizes or configured
limits; the runs are not a controlled performance comparison.

Legacy durable registry, epoch observation, asynchronous evidence and engine
checkpoint tests also passed with strict compiler warnings. The architecture
guard recursively checks nested headers, includes the application layer and
allows legacy host bridges by exact reviewed importing file and destination.
Native Windows uses the same tests and file protocol through Win32 locking,
`FlushFileBuffers` and write-through file replacement. Filesystem-specific
power-loss behavior is not certified by process-interruption tests.
