# Native history scale and lifecycle qualification

`anaiero qualify scale PROFILE NEW_OUTPUT [DEVICE]` and the controller's
`ce_native_qualification scale PROFILE NEW_OUTPUT [DEVICE]` run synthetic
throughput together with the actual source lifecycle, receiver retention and
canonical compaction services. The optional device declaration is retained and
hashed in the report. Use a new output directory for every attempt.

```text
ce_native_qualification scale qualification/native/scale-10k.cevqualification run-scale-10k device.cevdevice
ce_native_qualification scale qualification/native/scale-100k.cevqualification run-scale-100k device.cevdevice
ce_native_qualification scale qualification/native/scale-1m.cevqualification run-scale-1m device.cevdevice
```

The runtime is native C++20 on Windows and Linux. These commands require no
Python or external numerical solver. The compiled mesh **service** is used;
OpenSSL and a network transport are unnecessary for this workload. Its relay
identity is an explicitly synthetic authenticated fixture, so the resulting
report makes no TLS, certificate-provisioning or network-performance claim.

## What the campaign actually executes

One isolated canonical store holds all three workload namespaces:

1. **Retained throughput history.** Bounded batches append uniquely identified
   observations, payloads and receipts. A guarded cursor pins the accepted
   observation count. Throughput observations remain retained throughout the
   run so old, middle and recent historical lookup can be measured at each
   declared scale.
2. **Source churn.** A single active slot repeatedly registers, activates,
   executes, drains, seals and retires sources. Odd-numbered churn operations
   advance a returning UID's epoch; even-numbered operations introduce a new
   UID at epoch one. This measures both slot reuse and retained identity growth.
   Each epoch creates four observations through `lifecycle::Service::stage_commit`.
   Reviewed maintenance expires and physically reclaims the retired payloads,
   checkpoints and transition objects. A real reader pin first blocks one
   maintenance review; releasing it allows the explicit review to proceed.
3. **Receiver retention.** The receiver accepts each synthetic relay page under
   two overlapping subscriptions. It retains a configured page window. Once
   both cursors advance, reviewed receiver maintenance expires the oldest page
   and both receipt copies, then physically reclaims the objects. Exact old
   deliveries fail with `HISTORY_EXPIRED`; current cursor/projection data remains
   accessible. In-memory page retention is bounded by the configured window
   plus one newly received page.

At each checkpoint, the campaign churns the configured share of source epochs,
advances the receiver, inventories actual disk usage, reviews and executes
canonical compaction, inventories again, closes and reopens the store, probes
three historical regions, and performs a full canonical audit.

Compaction must preserve the exact terminal root/generation, the original first
throughput commit receipt, source ownership and retired-epoch fences, expired
receiver retry outcomes and the current receiver pages. Failed resurrection and
owner-change attempts must leave the canonical root unchanged. The active
lifecycle directory must remain empty after retirement, with its lifetime
observation count preserved.

The final separate 2 MiB quota control grows a real store until ordinary
admission is rejected. It verifies that rejection leaves authority unchanged,
that a bounded maintenance commit still fits in the reserved region, and that
maintenance cannot exceed the absolute quota. This is an actual **declared
quota** exercise; it is not physical filesystem exhaustion.

## Profiles and interpretation

| Profile | Throughput observations | Local source epochs | Checkpoints | Receiver pages per checkpoint |
|---|---:|---:|---|---:|
| `scale-ci` | 256 | 4 | 128, 256 | 4 × 4 observations |
| `scale-10k` | 10,000 | 10 | 1,000, 10,000 | 16 × 16 observations |
| `scale-100k` | 100,000 | 30 | 10,000, 100,000 | 32 × 16 observations |
| `scale-1m` | 1,000,000 | 100 | 10,000, 100,000, 1,000,000 | 64 × 16 observations |

Every profile retains two receiver pages. The instantaneous high-water mark is
three pages because the next page must commit before the old page becomes
eligible for expiry. Source and relay objects are additional to the declared
throughput observation count; the report records each population separately.

`ANAIEROSCALEQUALIFICATION/1` declares exact nearest-rank threshold-count
acceptance before execution. A p99 limit passes precisely when no more than
`floor(samples / 100)` observations exceed that inclusive limit. This uses
fixed storage and retains exact counts; logarithmic percentile upper bounds
remain descriptive. Each historical region is checked independently. This new
workload/version does not alter old synthetic profiles or reclassify their
original failures.

All fields are required, unique and bounded. Unknown fields, noncanonical
integers, a first checkpoint below six observations, oversized receiver pages,
an empty retention window, and work budgets exceeding total store quota are
rejected. A window cannot equal or exceed the number of pages generated per
checkpoint, ensuring that every checkpoint actually exercises reclamation.

## Measurements and capacity policy

Reports include:

- Exact source/profile/device witnesses, compiler/platform environment and
  the compiled build identity.
- Throughput commit duration and exact threshold counts. Lifecycle/receiver
  work is included in total elapsed time, not the throughput-only distribution.
- Indexed claim lookup in old, middle and recent regions, with bounded node
  visit counts. The first query in each region additionally verifies its full
  payload and receipt; that byte verification is outside the claim-lookup timer.
- Healthy reopen duration and node visits before historical probes, full audit
  duration/work, process resident/peak memory and process CPU time.
- Before/after primary-store regular-file logical and allocated bytes, file and
  directory counts, and directory allocation when the platform exposes it.
  Filesystem free bytes and available inode/file-node values describe the
  containing filesystem; they are not private capacity reservations.
- A final inventory of the entire output directory, including the separate
  quota-control store and pinned inputs, before the final report is written.
- Canonical compaction's declared temporary-space requirement, actual removed
  transaction/index bytes, retained record/blob/operation identity counts,
  summarized history, and remaining quota after cleanup.

Filesystem journals and unexposed metadata costs are not silently estimated.
The disk gate applies to regular-file allocation; directory allocation is
reported separately where available. Before/after inventories do not establish
the instantaneous peak during compaction. The preflight working-space bound is
reported as a conservative requirement, not a measured peak.

Before opening a store, the scale runner inventories the output filesystem.
It requires at least `7 × throughput_observations + 128` available file nodes
when the filesystem reports a finite pool: a retained payload and both old and
replacement sets of three observation index entries can coexist during
compaction. This is a known lower bound; other identities, receipts and
external writers still consume capacity. An insufficient pool produces a
failed report with the measurement, requirement and zero observations.
An unavailable file-node count stays explicitly unavailable. The writer also
checks headroom at individual commit and compaction admission.

`lifecycle::Service::capacity()` exposes a passive `CapacityStatus` at the
current canonical generation: accounted bytes, absolute quota, ordinary limit,
reserved maintenance bytes, ordinary and maintenance remaining bytes, pending
reclamation, and index/writer health. Remaining values saturate at zero when
usage exceeds a limit or the store cannot safely admit writes. They are
advisory accounting facts; the serialized writer still decides each admission.

Authority, source identities, tombstones and exact operation receipts remain
retained. Reclaiming payloads or compacting overwritten records does not create
permission to forget those identities. Arbitrary identity cardinality therefore
continues to consume finite quota. When those protections prevent enough
reclamation, ordinary admission stops explicitly. The page window bounds live
receiver payloads and active memory, not all permanent historical metadata.

The ordinary maintenance reserve also does not guarantee space for any possible
canonical compaction. A reviewed compaction must fit both its configured work
budget and the remaining absolute store quota, as well as available filesystem
space. A failure preserves the reviewed requirement in the failed report and
must be resolved through an explicit capacity policy or a newly declared run.

## Recovery and qualification boundaries

The normal scale campaign measures orderly close/reopen. Its native test also
combines an actual payload deletion interrupted before its accounting commit,
resumed maintenance, a canonical-base publication fault, resumed compaction,
and subsequent exact-receipt/source-identity checks. These are explicit software
fault cuts. They do not establish OS-crash or physical power-loss behavior.

The CI profile and negative work-budget control are short integration evidence.
A million-observation qualification requires the completed `scale-1m` report
on the declared platform/device. Profile availability alone is not that result.
Retain failed reports and their exact inputs alongside passing runs.
