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
| [Frozen 100k maintenance campaign](scale-100k-frozen/report.json) | **Passed** in 1,507.37 s on the declared shared Linux device. 100,000 observations, 30 epochs across 16 source IDs; 992 of 1,024 receiver observations expired. 12 of 1,588 commits exceeded 2 s (15 allowed); none of 1,020 lookups exceeded 250 ms. |
| [Short scale campaign](scale-ci/README.md) | Passed: 256 observations, 4 epochs across 3 source IDs; 32 received observations, 24 expired, 96 historical lookup samples. |
| [Synthetic million](synthetic-1m-admission/report.json) | Refused before store creation. At least 4,000,128 file nodes required; 2,908,702 available. |
| [Maintenance million](scale-1m-admission/README.md) | Refused before store creation. At least 7,000,128 file nodes required; 2,908,900 available. |

Neither million-profile preflight executed a workload or qualified latency.
At 100k, final compaction took 376.49 s and required 412,391,296 working bytes.
The store reopened in 20.10 ms and audit took 103.64 s. The full output tree
occupied 418,398 regular files (1,797,632,000 allocated bytes plus 44,474,368
directory bytes). Protected data, exact retries, quota rejection and the
maintenance reserve all passed. This is one maintenance profile on one declared
device; it does not override the separate synthetic 10k failure.

## Native tests and package transition

The [frozen Linux CTest results](native-tests/results.json) cover 92 registered
cases: 89 passed, three IPC cases skipped, no failures. Both CTest selections
used the same full Release build at `a8131574`; its whole-build identity
differs from the controller-only scale executable as recorded in the manifests.
The [post-correction Linux result](native-tests/portability.json) passed the
three directly affected deployment, compatibility and mixed-campaign cases
at `addf1fda`.

The [real installed Linux package pair](package/README.md) had different
executable and build identities and identical verified state witnesses. Four
accepted cycles survived reverse selection, then the previous executable
accepted cycle five and passed audit. This experiment used two compilations
differing only in the restricted display literal, with no OS service installed.

A separate [hosted Ubuntu systemd run](package-hosted-linux/README.md) did
exercise the actual service manager. The first executable reached 15 cycles,
the replacement reached 30 after the stale registration was rejected and
reconciled, and rollback retained all 30. The job removed the registration
and audited the retained store. Its archive records the PR checkout used.

The [corrected-source hosted package matrix](package-hosted-corrected/README.md)
then passed on both Ubuntu 24.04 and Windows Server 2022 using the actual
`addf1fda` push checkout. The systemd replacement increased completed
cycles from 27 to 53 and rollback preserved 53. On SCM the corresponding
counts were 12, 24 and 24. Both removed the registration and passed final
audit. The [original Windows compile failure](windows-package-build.md)
remains recorded separately.

## Hosted storage measurements

The [hosted Linux synthetic 100k report](hosted-synthetic-100k-linux/report.json)
passed the unchanged two-second limit: no commit out of 1,588 exceeded the
threshold, the legacy histogram bound also passed, and the full audit passed.
Elapsed time was 527.27 s. The exact job/checkout and device declaration
distinguish it from the shared-workspace maintenance run and the failed 10k.

The [isolated filesystem proofs](pressure-hosted/README.md) record Linux
ENOSPC (28) in tmpfs and Windows ERROR_DISK_FULL (112) on a fixed NTFS virtual
disk. Both rejected admission before a write, handled a separate real write
failure after admission, preserved the prior root, recovered, retried exactly
and passed audit. These are full-filesystem software tests. They do not
simulate controller cache loss, a physical power interruption, or prolonged
continuous service.
