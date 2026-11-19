# Comparing the two native history indexes

The native store supports an opt-in packed B+tree for its derived history
index. A leaf holds at most 32 ordered entries. The builder aims for 32 KiB
of row data per leaf and permits large individual values in bounded singleton
pages. A branch carries checked key ranges, child digests and subtree counts.
New writes replace the affected paths. Compaction constructs a complete second
index from sorted canonical history. The store verifies its coverage before
publishing it and reclaiming the old files. Both layouts use the same canonical transaction
and root publication. The packed implementation flushes and replaces individual
derived page files, while the single-node implementation flushes their directory
in a batch. Runtime comparisons include these differences in derived writes.

A new store uses the existing single-node layout unless an application
explicitly requests `IndexLayout::packed_pages`. Opening a store with automatic
layout selection reads the recorded format. The SDK also accepts a target
layout on a reviewed compaction for an existing store. Use the
[operator guide](../../native/docs/modular-lifecycle.md) for session maintenance.

## Measure a layout

Use the same Release executable, exact qualification profile and exact device
declaration for both layouts. Store each result in a new directory on the
measurement volume. The native runner takes these arguments:

```text
ce_native_qualification scale qualification/native/scale-10k.cevqualification output-a device.cevdevice --index-layout node-files
ce_native_qualification scale qualification/native/scale-10k.cevqualification output-b device.cevdevice --index-layout packed-pages
```

For a paired trial, run the following command from the repository root with
Python 3.11 or later. The comparison wrapper is an operator tool. The native
store and its qualification executable run independently of Python.

```text
python qualification/native/compare-index-layouts.py PATH_TO_EXECUTABLE qualification/native/scale-10k.cevqualification device.cevdevice NEW_OUTPUT
```

The default order is single-node, packed, packed, single-node. The two pairs
therefore have opposite order. `--order BAAB` reverses that sequence. The
wrapper retains the pinned inputs, executable digest, output streams, stores
and native reports, including failures. It writes `comparison.json` after
each run. A completed comparison has `complete=true`. Its `passed` field
also requires each native workload to pass, both pairs to have the same build
and sample counts, and the packed layout to pass each comparative criterion.
A dropped trial or capacity refusal cannot qualify a pair.

At each checkpoint the comparison checks for fewer steady regular files and
fewer regular files plus directories while both indexes exist. It also
requires the packed member of each pair to have no additional commits above
the profile’s latency limit. Read the per-run memory, allocated bytes,
compaction time, reopen and oldest/middle/recent lookup measurements alongside
that decision. A file-count improvement may come with a write-time cost.

The coexistence inventory covers the whole primary store after building and
verifying the replacement and before removing its predecessor. It reports old
and new index file counts separately. Directory counts exclude the store
root. This measures a specific high-water point. Shorter-lived temporary
files between inventory points and filesystem-internal metadata are outside
the measurement. The final output inventory includes the quota-control store
and other output files. Compare the same type of inventory across runs.

## Use the scale profiles

`scale-ci`, `scale-10k`, `scale-100k` and `scale-1m` use the existing
numeric budgets. Fresh packed admission requires at least one distinct
payload file per observation plus the reserve. Physical pages, receipts,
transactions and both index generations need additional file nodes and
bytes. Native commit and compaction admission check their own working
requirements at the operation. Device declarations identify the host and the
limits of its measurements. Hosted runs describe the named hosted environment.

The [comparison workflow](../../.github/workflows/native-index-comparison.yml)
runs the short paired trial and recovery fixtures on Windows and Linux. A
push changing the campaign definition also requests the 10k pairs, followed
by the 100k and million-observation packed maintenance profiles when prior
gates pass on that OS. Manual dispatch can select a smaller tier. Each
completed tier uploads the original inputs and reports. Inspect the actual
jobs and their artifacts for the largest completed tier.

For commit-by-commit attribution and a prescribed background I/O treatment,
see [the controlled load experiment](SLOW-COMMITS.md).
