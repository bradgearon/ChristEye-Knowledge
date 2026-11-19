# CE-SW-41 runtime and lifecycle evidence

These records were produced on a shared Linux development machine. Keep the
exact inputs and results together; a diagnostic against a working tree and
an acceptance run on a published commit are different claims. Earlier
[hosted and sandbox outcomes](../2026-11-18-operating-envelope/README.md)
retain their original decisions. The development date is November 18 in
America/Chicago; execution timestamps in the reports are UTC.

| Measurement | Observed result | Scope |
|---|---|---|
| [Synthetic 10k](synthetic-10k/report.json) | 10,000 observations, 100 source epochs; 0 of 159 commits exceeded 2 s. Audit and restart identity passed. | 116.03 s wall time. Of 52.90 s in measured commits, index publication used 33.15 s and blobs 16.22 s. This diagnosed where to reduce work. |
| [Scale 10k](scale-10k/report.json) | 10,000 observations, 10 epochs across 6 source IDs, 512 received observations and 480 expired. No commit exceeded 2 s; no sampled historical lookup exceeded 250 ms. | 200.51 s wall time. Last compaction took 64.26 s and required 86,956,848 working bytes; final output occupied 45,420 regular files. Accounts and allocations measure different things. |

A separate [five-engine campaign and TLS record](mixed/README.md) contains
32 actual cycles, 28 exact remote acknowledgments, both audited stores,
interrupted-attempt reports and exact replay after process termination. Its
storage-pressure check establishes safe refusal of a non-isolated workspace;
it did not fill that workspace or establish isolated-volume recovery.

These two runs were gathered during parallel development, before the final
index changes. Read [synthetic provenance](synthetic-10k/provenance.json) and
[scale provenance](scale-10k/context.json) for the exact executable fingerprints
and which files had changed since configure. They are not a controlled
before/after comparison or a claim about a dedicated production disk.

Run descriptions and acceptance criteria: [exact timing](../../native/README.md),
[retention and scale](../../native/SCALE.md), [mixed host and TLS](../../native/MIXED.md),
and [package lifecycle](../../../deployment/native/package-lifecycle.md).

## Frozen native build

The shared Linux runner used executable SHA-256
`623d66da6e6881c0c6cbba65cc9de2ea6931c20fc2e460cf67c16d9d583dea99`.
All 279 inputs in its whole-build manifest matched the frozen source at
launch. It records an operator declaration for the shared overlay and concurrent
work; the device declaration does not assert an exclusive or physical device.

| Profile | Result on this filesystem |
|---|---|
| [Frozen synthetic 10k](synthetic-10k-frozen/report.json) | **Failed** unchanged exact p99 limit: 4 of 159 commits exceeded 2 s, with only 1 allowed. Maximum 2.372 s. All 10k observations, 100 epochs, restart and full audit completed. Concurrent work was declared; its causal impact was not isolated. |
| [Short scale campaign](scale-ci/README.md) | Passed: 256 observations, 4 epochs across 3 source IDs; 32 received observations, 24 expired, 96 historical lookup samples. |
| [Synthetic million](synthetic-1m-admission/report.json) | Refused before store creation. At least 4,000,128 file nodes required; 2,908,702 available. |
| [Maintenance million](scale-1m-admission/README.md) | Refused before store creation. At least 7,000,128 file nodes required; 2,908,900 available. |

Neither million-profile preflight executed a workload or qualified latency.
