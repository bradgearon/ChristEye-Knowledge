# Shared-sandbox 100,000-observation preflight

**Result: failed the declared batch-commit p99 upper-bound budget.**

This is the completed local preflight from 2026-11-18, preserved as negative
operating evidence. The raw [report](report.json), [profile](profile.cevqualification)
and [compiled build manifest](build-manifest.txt) are unchanged. The native
executable returned exit status 2. The profile's thresholds were not relaxed.

The run used a pre-publication working-tree build, identified by the exact
manifest and the report's build identity. It is not assigned the final PR head
commit and does not qualify that release. The local workspace was shared with
concurrent compilation and integration checks; CPU and storage contention were
not controlled. The profile's original generic CI storage label is retained
verbatim, while this record identifies the actual shared-sandbox setting.

| Measurement | Observed result | Declared gate |
|---|---:|---:|
| Throughput observations / retired epochs | 100,000 / 100 | 100,000 / 100 |
| Active lifecycle sources | At most 1 | Bounded workload |
| Batch commit p99 upper bound | 2.147483648 s | At most 2 s — **failed** |
| Batch commit maximum | 3.882630264 s | Reported, no separate maximum gate |
| Historical lookup maximum index visits | 20 | At most 128 |
| Lookup p99 upper bound at 100,000 | 8.388608 ms | At most 250 ms |
| Healthy reopen at 100,000 | 1.112128 ms; 1 index node | At most 5 s and 128 nodes |
| Process peak resident bytes | 10,309,632 | At most 536,870,912 |
| Final accounted bytes | 278,740,108 | At most 2,147,483,648 |
| Final allocated regular-file bytes | 1,750,413,312 | At most 4,294,967,296 |
| Final regular-file count | 416,037 | At most 2,000,000 |
| Full canonical audit | Passed; 258.256102337 s | Correctness required |
| Total elapsed time | 1,097.780260358 s | Includes ingestion, inventory, churn and audit |

Recovery identity, missing-sequence behavior, ownership/retirement fences and
the final audit passed. The only reported budget violation is the batch-commit
p99 upper bound. The fixed base-2 histogram supplies conservative upper bounds;
2.147 seconds is not an exact interpolated percentile. This report does not
establish why the tail exceeded its gate or prove that removing contention
would make the same environment pass.

The write path recorded 1,367,159 index-node writes and 6,173,056 elided transient
nodes, with zero staging spills. These are direct work counters, not a controlled
before/after speedup experiment. Full audit remains an explicitly measured
historical scan; it is distinct from indexed point lookup and healthy startup.

The large generated canonical store is not committed to the repository. Hosted
Windows/Linux reports are separate evidence for their exact commit and runner.
The million-observation, mixed 24-hour/seven-day, OS-crash and device power-loss
gates remain unachieved by this preflight.
