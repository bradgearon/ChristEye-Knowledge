# CE-SW-42 — Measuring and reducing the file cost of retained history

The earlier shared Linux 10k run had four commits over two seconds when its p99
budget allowed one. Its aggregate stage measurements could not identify which
operations made individual batches slow. At 100k observations, one maintained
store and its qualification outputs occupied 418,398 regular files. These
observations motivated an instrumented replay and a second index representation.
[Earlier operating results](../../qualification/reports/2026-11-18-runtime-lifecycle/README.md)

| Change | What an operator can establish |
|---|---|
| Per-batch latency attribution | Inspect the slowest 32 observed attempts with their operation, measured commit stages and index work. The report retains exact counts for the whole run and shows omitted slow attempts. Replay the same executable with baseline and prescribed I/O load. |
| Optional packed index | Store multiple sorted rows in a checksummed leaf and update the necessary branch paths. Older data can be opened, converted through reviewed compaction and reopened. |
| Paired layout comparison | Compare both orders on one device using identical executable/profile/device bytes. Measure live file counts with both complete indexes present, as well as ordinary disk usage, latency, memory and reopen cost. |
| Recovery and scale gates | Exercise real older store bytes, interrupted construction and publication, damaged pages, repair, pins, expiry and exact retries. Attempt the original 100k and 1m profiles when earlier gates pass. |

## Reading the results

Each layout creates a distinct store with its own identity. Compare matching
workloads and sample populations. Compare the canonical root and exact receipt
within a store across restart, conversion and rollback. An operating claim
belongs to the build, device, profile and duration recorded in that result.
A smaller derived index can still miss a commit deadline or require more memory.

[Layout commands and acceptance rules](../../qualification/native/PACKED-INDEX.md)
· [Controlled replay](../../qualification/native/SLOW-COMMITS.md)
· [Recovery fixture provenance](../../native/tests/fixtures/pre-packed-index.md)

[Observed measurements and source scope](../../qualification/reports/2026-11-19-packed-index/README.md)
