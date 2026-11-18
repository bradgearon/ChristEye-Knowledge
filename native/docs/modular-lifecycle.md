# Modular lifecycle, retention and restore

This is the implemented M4 contract for the compiled modular session path.
`anaiero_lifecycle` contains portable domain services and only sees installed
SDK headers. `anaiero_storage_native` implements immutable archives and native
store restoration. The public API is
[`lifecycle.hpp`](../sdk/include/anaiero/sdk/lifecycle.hpp).
The generated [installed SDK reference](sdk-reference.md) includes exact public
declarations and their source hashes.

These services use the **same canonical commit store** as engine state and
operation receipts. They do not maintain a second authority journal. They are
owner serialized, use record revision/digest guards, and have no hidden timer,
transport loop or background lease sweep. The existing CE-SW-35 mesh remains a
separate compatibility path; continuous delivery and connection management are
M5 work.

Stable operation IDs bind the operator's semantic request, including explicitly
requested lease deadlines. Host admission time is recorded separately. Retrying
the same request later resolves its original receipt, including after the grant
expires; it does not renew that grant or run a new sweep. A later sweep or changed
request needs a new operation ID.

## Source lifecycle and active checkpoints

The allowed progression is `registered -> active -> draining -> sealed ->
retired`. Registration pins the complete engine identity and source owner. A
source UID has an immutable owner and clock domain. Epochs advance consecutively
after the preceding epoch is retired; the durable closed-epoch fence prevents
reopening an old epoch.

The runtime calls `Service::stage_commit` on the engine's canonical batch.
Initial attachment binds an exact observation-free checkpoint, including its
revision and tick. Later transitions must continue the stored state digest and
revision. The same commit contains source sequencing, observation claims,
transition receipts and the active checkpoint. A prepared transition that loses
the source/active revision race to a drain barrier cannot commit afterward.

Sealing requires the exact terminal sequence and payload digest. Retirement
removes the source and its metrics from the active checkpoint, while retaining
its historical catalog record and closed-epoch fence. The session then calls
`SessionService::remove_retired` to release the live engine and its current
engine/metric roots. Explicit recovery, module, program and backup pins remain
protective until released.

Checkpoint schema 2 separates view revision, lifetime observation count, active
sources and active metrics. An empty active view with nonzero lifetime history
is valid. Historical lookup uses the canonical store's ordered index rather
than rebuilding all historical sources into the live view.

Observation payloads use `encode_source_observation` and
`decode_source_observation`: their identity includes source UID, epoch and
sequence as well as the metric payload. Equal numerical values in two streams
therefore do not share an expiry identity accidentally. This is a new SDK
payload envelope, not a change to the legacy mesh wire format.

Checkpoints similarly distinguish logical `state_digest(state)` from the
stored `checkpoint_digest(identity, state)`. Source records keep both
`state_sha256` and `state_object_sha256`. Current roots and bootstrap storage
use the source-bound object identity. Expiring a retired epoch's checkpoint
therefore cannot prevent a new source/epoch from starting with an otherwise
identical logical state. Bootstrap's logical snapshot bytes and digest remain
separate from its stored checkpoint envelope.

Default service limits are 32 live source entries, 256 live metrics, 32 active
peers, 32 active subscriptions, 64 pins and 64 objects per pin or maintenance
request. Canonical batch/blob/reference limits also apply; reaching any bound
returns capacity before publication. The maximum admitted peer/subscription
lease is 30 days.

## Peer authority, offers and acknowledgements

Peer updates require an exact prior generation and the next generation.
Rotation, suspension and revocation commit an ordered barrier and invalidate
that peer's existing subscriptions. `expire_peer_leases` releases expired peer
directory entries and their subscriptions; `expire_subscriptions` releases
expired subscription leases. Call these sweeps explicitly from the operator or
host. An expired credential cannot grant access while waiting for a sweep.

A subscription binds one principal and one source/epoch. Renewal preserves its
acknowledged cursor and advances its generation. `offer` records one exact
canonical cursor; `acknowledge` accepts that offer or the already acknowledged
cursor. A stale generation, forged digest, unoffered cursor, expired lease or
revoked principal cannot advance the durable cursor. There is no implicit ACK
for unavailable history.

Each authorized subscription operation includes the principal, exact scope,
credential generation and the earlier of peer/subscription expiry in its
canonical authorization guard. The native writer checks its admission clock
again immediately before irreversible writing and records admission time in
the receipt. Its durable clock floor rejects rollback. `IAdmissionClock` is
injectable for deterministic tests; the default native implementation uses UTC.

## Protected roots and retention

`put_pin` and `release_pin` use pin generations and update the protection
revision in the common commit domain. Pins reference existing, unexpired
objects. **A deadline alone does not release a pin.** In particular a live
reader retains its objects until its owner explicitly releases the lease.

Maintenance is a three-step operator operation:

1. `review_maintenance` captures the source tip, protection/subscription
   revisions, selected object identities, byte count and proposed replay floor.
   The source must be sealed or retired. Objects must have canonical ownership
   witnesses in that source's history. Active subscriptions that still need the
   range and explicit pins prevent review.
2. `activate_maintenance` revalidates source, pins, subscriptions, archive and
   revisions. Its single canonical transaction records the prune manifest,
   interval availability and new replay floor, and marks the objects expired.
   The store independently rejects any object still referenced by a current
   record or live blob graph. **No payload is physically deleted first.**
3. `execute_maintenance` reclaims those canonically expired payload files and
   records completion. An interrupted execution can resume from the activated
   manifest. Historical observation identity and expiry witnesses remain
   indexed after payload deletion.

Archiving requires a configured `IArchiveStore` whose identifier matches the
request. Review writes each selected object and verifies its read-back digest;
activation verifies the archive again. Native archives have an exclusive
writer, immutable digest-addressed files, per-object bounds and their own byte
quota. **The archive quota is separate from the canonical store quota.**

`replay_status` returns `ok`, `history_expired`, `archive_required`, or
`archive_unavailable` for the next relevant boundary. Indexed interval records
preserve earlier expired gaps when a later interval is archived. An archive
receipt is not a promise of permanent external storage availability.

This implementation reclaims **blob payloads** and obsolete derived index
nodes. It does **not** prune canonical journal prefixes or discard the retained
historical catalog/operation metadata. Those records consume the finite store
quota. Admission stops explicitly when remaining protected data and metadata
leave insufficient headroom. Unlimited-duration operation within fixed disk
space is not an implemented claim; a later canonical compaction/receipt-expiry
policy is required for that operating profile.

Ordinary writes leave the configured maintenance reserve. Drain/seal/retire,
revocation, lease expiry/release, retention manifests and quarantine recovery use
the maintenance commit class, so ordinary capacity exhaustion does not consume
their reserved headroom. Maintenance still has to fit the absolute quota and
bounded index-writing workspace; the reserve is finite, not an override.

## Bootstrap and restored stores

`accept_bootstrap` requires a current authorized principal, a bounded lease,
the next subscription generation, exact canonical source tip and exact decoded
engine snapshot. It commits the snapshot root and subscription cursor together.
`installed_bootstrap` reads that installed snapshot/cursor after restart. A
bootstrap is recorded as a new-generation state installation; it does not
pretend that missing events were delivered. An expired snapshot cannot silently
be resurrected under its previous digest.

`restore_quarantined_store(source, destination, operation, report, options)` is
the native backup/restore entry point:

1. Acquire the source's existing native writer lock. An active source is
   rejected; source data files are read without opening or repairing its store.
2. Copy into private staging under the destination parent. Bound entry count,
   depth, individual file size, total bytes and required disk headroom; reject
   symlinks and special files. Read back every copied file.
   The persisted restore intent contains a fresh nonce: independent copies have
   distinct incarnations even if their caller operation IDs happen to match,
   while retrying the same intent preserves its incarnation.
3. Open and fully audit the clone, repairing its derived index if needed.
   Commit the lifecycle quarantine barrier inside staging.
4. Close the clone and publish its directory with an atomic **no replacement**
   rename, followed by parent-directory flushing. Existing destinations are
   never replaced. A matching retained intent reconciles a retry after an
   uncertain publication. Failures retain private staging for diagnosis/retry.

The report exposes copied file/byte counts, source generation/root, new
incarnation, staging location and publication state. It copies the native store
tree. Executables, archives and dependencies outside that tree must be retained
and verified separately. Directory durability on Windows remains subject to
the host filesystem's documented guarantees.

Quarantine blocks engine execution and peer-authorized operations. The operator
must explicitly call `reauthorize_restore` with the digest of their current
policy declaration. Reauthorization atomically retires restored live sources,
clears their metrics, and invalidates all restored peer/subscription grants.
Fresh peer enrollment needs a new credential and generation. Restored
subscriptions require explicit bootstrap. New source UIDs must begin with
`<snapshot.incarnation>.`, so unknown post-backup epochs are not silently reused.
The same reauthorization commit captures the original engine identities in a
bounded cleanup record. `reauthorization_sources(operation_id)` returns those
identities on retry even after the active view or session configuration changed;
the host can finish detaching the original retired engine roots after a crash.
It must compare the full identity before detaching an instance ID reused by a
new source.

The policy declaration is a local operator decision, **not cryptographic proof
that the backup was the newest root**. Automatic external anti-rollback witness
discovery is not implemented.

## Operator composition and validation

`ce_modular_session admin STORE COMMAND_FILE [ARCHIVE_DIRECTORY]` exposes source,
peer, subscription, pin, maintenance and recovery administration. Each command
file contains one operation and a stable operation ID. Use source inspection to
obtain the exact tip before sealing. Keep maintenance review, activation and
execution as separate operations so changed protections can reject activation.
The public SDK exposes the same typed operations for other native hosts.

### Exact command-file grammar

The command file is at most 16 KiB and contains exactly one command; additional
tokens are rejected. Names below in uppercase are parameters. `OP` is a stable
operation ID. Generations, epochs, sequences, counts and Unix timestamps are
unsigned decimal integers. Digests use lowercase hexadecimal SHA-256 text.
Use `-` for the empty digest only when the corresponding cursor sequence is 0.

| Operation | Command-file contents |
|---|---|
| Inspect source and terminal cursor | `source inspect UID EPOCH` |
| Stop source admission | `source drain UID EPOCH OP` |
| Seal exact terminal cursor | `source seal UID EPOCH SEQUENCE PAYLOAD_SHA256 OP` |
| Retire and detach | `source retire UID EPOCH OP` |
| Enroll, rotate, suspend or revoke peer | `peer upsert PRINCIPAL EXPECTED_GENERATION NEXT_GENERATION CERT_SHA256 EXPIRES_UNIX_S PHASE OP` |
| Expire peer grants and their subscriptions | `peer expire OP` |
| Open or renew subscription, preserving ACK progress | `subscription open ID PRINCIPAL UID EPOCH NEXT_GENERATION LEASE_UNTIL_UNIX_S OP` |
| Record one offered cursor | `subscription offer ID GENERATION SEQUENCE PAYLOAD_SHA256 OP` |
| Acknowledge the offered cursor | `subscription ack ID GENERATION SEQUENCE PAYLOAD_SHA256 OP` |
| Close a subscription | `subscription close ID GENERATION OP` |
| Expire subscription leases | `subscription expire OP` |
| Install exact snapshot and cursor in a new generation | `bootstrap accept ID PRINCIPAL UID EPOCH NEXT_GENERATION LEASE_UNTIL_UNIX_S SEQUENCE PAYLOAD_SHA256 "SNAPSHOT_FILE" OP` |
| Create or renew protected root | `pin put ID OWNER EXPECTED_GENERATION NEXT_GENERATION KIND EXPIRES_UNIX_S COUNT DIGEST... OP` |
| Release protected root | `pin release ID GENERATION OP` |
| Review selected history objects | `maintenance review ID UID EPOCH EXPIRE_THROUGH DISPOSITION "ARCHIVE_DIRECTORY" COUNT DIGEST... OP` |
| Commit expiry manifest and replay boundary | `maintenance activate ID OP` |
| Reclaim canonically expired payloads | `maintenance execute ID OP` |
| Inspect the next replay boundary | `history status UID EPOCH AFTER_SEQUENCE` |
| Reauthorize a quarantined restore | `restore reauthorize "POLICY_DECLARATION_FILE" OP` |

`PHASE` is `active`, `suspended` or `revoked`. `NEXT_GENERATION` is 1 for a new
identity, otherwise exactly the prior generation plus 1. Subscription renewal
loads the prior acknowledged cursor automatically; it cannot advance that
cursor. A later ACK does not change a previously committed renewal's operation
identity.

`KIND` is `0` recovery, `1` program, `2` module, `3` operation, `4` reader, `5`
backup or `6` operator pin. Pin expiry `0` means an explicit permanent root;
finite pin deadlines still require explicit release. `COUNT` is between 1 and
64 and must match the following digest tokens exactly.

`DISPOSITION` is `expire` or `archive`. For explicit expiry, use `""` for the
archive directory. For archiving, supply the configured archive path both in
the command and as the CLI's optional `ARCHIVE_DIRECTORY` argument. The service
requires the resolved configured archive identity. Quoted paths follow C++
`std::quoted` escaping: within a quoted path, escape a backslash as `\\` and a
literal quotation mark as `\"`.

The bootstrap snapshot file contains canonical `encode_state` bytes. Obtain it
with `ce_modular_session export-state STORE INSTANCE FILE` while that exact
source state is available, preferably after sealing and before detaching. The
bootstrap command computes its digest and verifies the complete current source
identity, state and cursor. The policy declaration is a nonempty file of at most
16 KiB; its digest records the operator's explicit recovery decision.

Other related top-level commands are:

```text
ce_modular_session status STORE
ce_modular_session audit STORE
ce_modular_session repair-index STORE
ce_modular_session capacity STORE MAXIMUM_BYTES OP
ce_modular_session restore SOURCE_STORE NEW_STORE OP
```

Restore requires the source to be quiescent and the destination to be absent.
Inspect the resulting quarantine/incarnation before supplying a current policy
declaration. A pending session cycle must finish before ordinary source lifecycle
administration can drain, seal or retire its source.

`anaiero_lifecycle_tests` exercises actual native files: repeated source churn
under a smaller active cap, empty-active/nonzero-history restart, a staged write
losing to drain, stale/false ACKs, lease and clock checks, pin races, a crash after
expiry-root publication, physical reclamation, mixed expired/archived ranges,
snapshot/cursor bootstrap, quarantine and fresh enrollment. Native restore tests
also verify unchanged source bytes, active-source refusal, clone restart,
reauthorization, retry, no-overwrite, quotas, symlink rejection and corruption
rejection. Linux execution passed during this implementation. Windows code is
implemented; native Windows execution is a separate CI qualification result.
