# Native operating-envelope evidence

This directory retains completed native measurements from the CE-SW-40
continuation. Every result names its runtime revision, actual CI checkout,
workflow/job/artifact, exact profile and compiled source/build witness.
Passing and failing reports retain their original bytes and original `passed`
flags. They are measurements of the listed executions, not a guarantee for
other hardware or an assertion that prolonged release qualification is complete.

## Final runtime checkpoint

Runtime `aec1489db375a910d44ee6ad037e321cc9b67e74` completed all four tracked
100k executions: PR Linux, PR Windows and push Windows passed; push Linux failed
only its conservative commit-p99 upper-bound gate. That failed run reported
2.147483648 seconds against the unchanged 2-second limit, with an exact maximum
of 3.543940010 seconds. Its audit, recovery, memory, disk and lookup gates passed.
The histogram does not disclose the exact p99 inside its selected bucket.

Hosted latency qualification remains **unresolved**. The passing and failing
Linux reports have the same compiled build identity. Elapsed/CPU differences
alone do not establish a cause, and neither favorable runs nor a tighter
estimator replace failed evidence. The next diagnostic work is exact counts
above predeclared latency limits, durability-stage timing and a declared device
profile before advancing the operating-envelope claim.

Functional verification at this runtime is complete: [full distribution
suites](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44425520929)
executed 81/81 on both platforms; [modular suites](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44425520890)
executed 80/80 with Python discovery disabled; [integrated sessions](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44425520892)
executed 12/12 on each platform; [real TLS/IPC mesh](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44425520882)
executed 9/9 on each; and [architecture checks](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44425520891)
executed 12/12 on each. These overlapping suites had no skipped CTest entries;
their counts are not added into a unique-test total. The separate Cloudflare
Workers check remained failed and does not establish website deployment health.

## Scope and unchanged workload

The synthetic profile commits **100,000 throughput observations**, an additional
**100 lifecycle observations**, and **100 retired epochs**, with at most one
active lifecycle source. It performs indexed historical queries, healthy reopen,
retirement checks and a full canonical audit. This is distinct from the short
five-engine continuous-host workload below; it is not 100,000 mixed-engine cycles.

All synthetic runs here retain the same profile SHA-256:

```text
40b24cc47571b3b348a88106943d8657c4ad5bd4ab9522182243998f8accb76e
```

The commit-p99 upper-bound limit remains **2,000 ms**. No profile threshold was
changed after an outcome. The synthetic reports contain memory, disk, lookup,
reopen and audit checks, plus measured total duration. Mixed profiles also
enforce their declared elapsed-duration budgets.

The runners report ephemeral CI storage with hardware unspecified. Linux and
Windows, and separate push/PR jobs, have different observed timing and allocation
costs. Do not treat these runs as a controlled hardware speed comparison.

## Percentile methods

Earlier reports use the original fixed 65-bucket logarithmic ceiling. Its
conservative p99 bound can exceed both a declared limit and the slowest observed
sample. Those original failures remain failures in the retained reports.

Revision `aec1489db375a910d44ee6ad037e321cc9b67e74` adds the explicit method
`log2-ceiling-capped-by-observed-maximum/1`: the reported upper bound is the smaller
of the selected logarithmic bucket ceiling and the exact observed maximum.
Both quantities bound the same nearest-rank observation, so their minimum is
still conservative. The histogram retains 65 counters; it does not retain all
samples or compute an interpolated percentile.

The sorted-sample native oracle checks all percentiles 1–100 over integer and
power-of-two edges, zero samples, UINT64_MAX, varying counts, a genuinely
over-budget p99 and an outlier outside p99. A smaller value under the new
estimator is not itself evidence of a runtime speedup. Old report fields are
never recalculated under the new method.

## Completed synthetic measurements

Times below are milliseconds (rounded to three decimal places) except total
wall time in seconds. The report links retain exact integer nanoseconds.
`Ceiling` means the original method; `Capped` means the explicit later method.

| Runtime | Trigger / platform | Method | Commit p99 upper (ms) | Exact maximum (ms) | Wall (s) | Original result | Evidence |
|---|---|---|---:|---:|---:|---|---|
| [`c196e56`](https://github.com/bradgearon/ChristEye-Knowledge/commit/c196e56ba71428de4d568217c542bf2f635cb005) | PR / Windows | Ceiling | 2,147.484 | 2,315.868 | 1,019.712 | **Fail** | [report](../2026-11-18-hosted-windows-100k/report.json) / [provenance](../2026-11-18-hosted-windows-100k/provenance.json) / [archive](../2026-11-18-hosted-windows-100k/artifact.zip) |
| [`fc93d15`](https://github.com/bradgearon/ChristEye-Knowledge/commit/fc93d15a9955b63617b8baf1321bacb25471c531) | PR / Linux | Ceiling | 268.435 | 225.928 | 290.499 | **Pass** | [report](fc93d15/pr-linux/synthetic/report.json) / [provenance](fc93d15/pr-linux/provenance.json) / [archive](fc93d15/pr-linux/artifact.zip) |
| [`fc93d15`](https://github.com/bradgearon/ChristEye-Knowledge/commit/fc93d15a9955b63617b8baf1321bacb25471c531) | PR / Windows | Ceiling | 2,147.484 | 1,656.423 | 971.083 | **Fail** | [report](fc93d15/pr-windows/synthetic/report.json) / [provenance](fc93d15/pr-windows/provenance.json) / [archive](fc93d15/pr-windows/artifact.zip) |
| [`fc93d15`](https://github.com/bradgearon/ChristEye-Knowledge/commit/fc93d15a9955b63617b8baf1321bacb25471c531) | PUSH / Linux | Ceiling | 536.871 | 456.974 | 515.718 | **Pass** | [report](fc93d15/push-linux/synthetic/report.json) / [provenance](fc93d15/push-linux/provenance.json) / [archive](fc93d15/push-linux/artifact.zip) |
| [`fc93d15`](https://github.com/bradgearon/ChristEye-Knowledge/commit/fc93d15a9955b63617b8baf1321bacb25471c531) | PUSH / Windows | Ceiling | 1,073.742 | 884.020 | 751.378 | **Pass** | [report](fc93d15/push-windows/synthetic/report.json) / [provenance](fc93d15/push-windows/provenance.json) / [archive](fc93d15/push-windows/artifact.zip) |
| [`bdc29758`](https://github.com/bradgearon/ChristEye-Knowledge/commit/bdc29758d5896d56d4f59122d78f909c404fd3c5) | PR / Linux | Ceiling | 268.435 | 219.271 | 292.745 | **Pass** | [report](bdc29758/pr-linux/synthetic/report.json) / [provenance](bdc29758/pr-linux/provenance.json) / [archive](bdc29758/pr-linux/artifact.zip) |
| [`bdc29758`](https://github.com/bradgearon/ChristEye-Knowledge/commit/bdc29758d5896d56d4f59122d78f909c404fd3c5) | PR / Windows | Ceiling | 2,147.484 | 1,379.902 | 769.786 | **Fail** | [report](bdc29758/pr-windows/synthetic/report.json) / [provenance](bdc29758/pr-windows/provenance.json) / [archive](bdc29758/pr-windows/artifact.zip) |
| [`bdc29758`](https://github.com/bradgearon/ChristEye-Knowledge/commit/bdc29758d5896d56d4f59122d78f909c404fd3c5) | PUSH / Linux | Ceiling | 1,073.742 | 1,693.941 | 548.322 | **Pass** | [report](bdc29758/push-linux/synthetic/report.json) / [provenance](bdc29758/push-linux/provenance.json) / [archive](bdc29758/push-linux/artifact.zip) |
| [`bdc29758`](https://github.com/bradgearon/ChristEye-Knowledge/commit/bdc29758d5896d56d4f59122d78f909c404fd3c5) | PUSH / Windows | Ceiling | 2,147.484 | 1,624.581 | 939.922 | **Fail** | [report](bdc29758/push-windows/synthetic/report.json) / [provenance](bdc29758/push-windows/provenance.json) / [archive](bdc29758/push-windows/artifact.zip) |
| [`6bc9d535`](https://github.com/bradgearon/ChristEye-Knowledge/commit/6bc9d5351f4648ed49746a445bcfffa0aba0dd80) | PR / Linux | Ceiling | 536.871 | 398.502 | 490.423 | **Pass** | [report](6bc9d535/pr-linux/synthetic/report.json) / [provenance](6bc9d535/pr-linux/provenance.json) / [archive](6bc9d535/pr-linux/artifact.zip) |
| [`6bc9d535`](https://github.com/bradgearon/ChristEye-Knowledge/commit/6bc9d5351f4648ed49746a445bcfffa0aba0dd80) | PR / Windows | Ceiling | 1,073.742 | 887.859 | 794.174 | **Pass** | [report](6bc9d535/pr-windows/synthetic/report.json) / [provenance](6bc9d535/pr-windows/provenance.json) / [archive](6bc9d535/pr-windows/artifact.zip) |
| [`6bc9d535`](https://github.com/bradgearon/ChristEye-Knowledge/commit/6bc9d5351f4648ed49746a445bcfffa0aba0dd80) | PUSH / Linux | Ceiling | 268.435 | 238.606 | 300.671 | **Pass** | [report](6bc9d535/push-linux/synthetic/report.json) / [provenance](6bc9d535/push-linux/provenance.json) / [archive](6bc9d535/push-linux/artifact.zip) |
| [`6bc9d535`](https://github.com/bradgearon/ChristEye-Knowledge/commit/6bc9d5351f4648ed49746a445bcfffa0aba0dd80) | PUSH / Windows | Ceiling | 1,073.742 | 3,776.110 | 790.956 | **Pass** | [report](6bc9d535/push-windows/synthetic/report.json) / [provenance](6bc9d535/push-windows/provenance.json) / [archive](6bc9d535/push-windows/artifact.zip) |
| [`aec1489d`](https://github.com/bradgearon/ChristEye-Knowledge/commit/aec1489db375a910d44ee6ad037e321cc9b67e74) | PR / Linux | Capped | 1,073.742 | 1,580.691 | 568.533 | **Pass** | [report](aec1489d/pr-linux/synthetic/report.json) / [provenance](aec1489d/pr-linux/provenance.json) / [archive](aec1489d/pr-linux/artifact.zip) |
| [`aec1489d`](https://github.com/bradgearon/ChristEye-Knowledge/commit/aec1489db375a910d44ee6ad037e321cc9b67e74) | PR / Windows | Capped | 882.670 | 882.670 | 585.418 | **Pass** | [report](aec1489d/pr-windows/synthetic/report.json) / [provenance](aec1489d/pr-windows/provenance.json) / [archive](aec1489d/pr-windows/artifact.zip) |
| [`aec1489d`](https://github.com/bradgearon/ChristEye-Knowledge/commit/aec1489db375a910d44ee6ad037e321cc9b67e74) | PUSH / Linux | Capped | 2,147.484 | 3,543.940 | 722.388 | **Fail** | [report](aec1489d/push-linux/synthetic/report.json) / [provenance](aec1489d/push-linux/provenance.json) / [archive](aec1489d/push-linux/artifact.zip) |
| [`aec1489d`](https://github.com/bradgearon/ChristEye-Knowledge/commit/aec1489db375a910d44ee6ad037e321cc9b67e74) | PUSH / Windows | Capped | 1,790.840 | 1,790.840 | 1,001.671 | **Pass** | [report](aec1489d/push-windows/synthetic/report.json) / [provenance](aec1489d/push-windows/provenance.json) / [archive](aec1489d/push-windows/artifact.zip) |

All failures in the table are retained as reported, including pre-optimization
failure evidence. Consult `violations`, the exact maximum and the method before
interpreting a latency failure. Passing one profile does not erase another run.
The configured gate applies to p99: a maximum above 2,000 ms can coexist with a
passing p99 when a small number of outliers fall above that percentile. Exact
maxima remain visible independently of the configured percentile gate.

## Short mixed-host measurements

The actual `NativeSession` and `NativeContinuousHost` execute Cell Behavior
(SW5), Multi-Colony (SIM3), the finite VLM program, passive voltage and scalar
environment. Every completed short profile runs 32 host cycles: the cell and
voltage clocks reach 32, colonies reach 16, the VLM completes at 12, and the
paused/resumed environment reaches 30.

Each retained positive report verifies queue rejection under bounded storage
pressure, exact durable control retry, explicit quota rejection, historical
probes, orderly restart identity and full audit. Its companion negative report
uses a deliberately impossible 1 MiB resident-memory limit and must fail that
gate while preserving the report. This negative control is distinct from an
operating-profile failure in the synthetic table above.

| Runtime / run | Host cycles | Positive wall (s) | Positive outcome | Negative control | Evidence |
|---|---:|---:|---|---|---|
| `bdc29758` / pr-linux | 32 | 6.206 | Pass, all six checks true | Fails only the intended RSS limit | [positive](bdc29758/pr-linux/mixed/report.json) / [negative](bdc29758/pr-linux/mixed/negative/report.json) |
| `6bc9d535` / pr-linux | 32 | 10.362 | Pass, all six checks true | Fails only the intended RSS limit | [positive](6bc9d535/pr-linux/mixed/report.json) / [negative](6bc9d535/pr-linux/mixed/negative/report.json) |
| `6bc9d535` / pr-windows | 32 | 15.557 | Pass, all six checks true | Fails only the intended RSS limit | [positive](6bc9d535/pr-windows/mixed/report.json) / [negative](6bc9d535/pr-windows/mixed/negative/report.json) |
| `6bc9d535` / push-linux | 32 | 6.333 | Pass, all six checks true | Fails only the intended RSS limit | [positive](6bc9d535/push-linux/mixed/report.json) / [negative](6bc9d535/push-linux/mixed/negative/report.json) |
| `6bc9d535` / push-windows | 32 | 15.374 | Pass, all six checks true | Fails only the intended RSS limit | [positive](6bc9d535/push-windows/mixed/report.json) / [negative](6bc9d535/push-windows/mixed/negative/report.json) |
| `aec1489d` / pr-linux | 32 | 9.952 | Pass, all six checks true | Fails only the intended RSS limit | [positive](aec1489d/pr-linux/mixed/report.json) / [negative](aec1489d/pr-linux/mixed/negative/report.json) |
| `aec1489d` / pr-windows | 32 | 11.777 | Pass, all six checks true | Fails only the intended RSS limit | [positive](aec1489d/pr-windows/mixed/report.json) / [negative](aec1489d/pr-windows/mixed/negative/report.json) |
| `aec1489d` / push-linux | 32 | 12.212 | Pass, all six checks true | Fails only the intended RSS limit | [positive](aec1489d/push-linux/mixed/report.json) / [negative](aec1489d/push-linux/mixed/negative/report.json) |
| `aec1489d` / push-windows | 32 | 18.887 | Pass, all six checks true | Fails only the intended RSS limit | [positive](aec1489d/push-windows/mixed/report.json) / [negative](aec1489d/push-windows/mixed/negative/report.json) |

## Exact source and artifact witnesses

Each run directory retains the original CI ZIP, selected byte-exact members,
and `provenance.json`. Provenance records the workflow and job, artifact identity,
ZIP SHA-256, each member's original archive path/length/hash, runtime branch head,
and actual checkout SHA. Pull-request jobs normally test a merge ref; their
checkout is recorded separately from the branch head. Push jobs record the
branch commit they actually checked out.

`build-manifest.txt` preserves the original file bytes. On Windows, CMake writes
CRLF bytes to that file while its in-memory `string(SHA256)` build identity is
computed from LF bytes. Such captures also retain `build-manifest.lf.txt`, labeled
as an explicit **CRLF-to-LF-only derivation**. Its hash matches the compiled build
identity; the raw manifest's distinct hash remains recorded. These files are
protected against Git newline conversion. The derivation does not alter the
source-file digests contained inside the manifest.

Full mixed captures also retain the exact profile, original session text,
binary canonical `session.definition`, positive report, and negative-control
profile/report. Their reported profile/session/definition hashes were checked
against retained bytes. The original ZIP includes CTest logs and other uploaded
verification files without depending on continued hosted-artifact retention.

The synthetic JSON printed in each job log was compared with the original
artifact report. The only permitted transport normalization was a trailing CRLF
log newline to LF; the archived report itself was never normalized or rewritten.

## Limits and next evidence

These are short functional and 100k synthetic measurements on hosted CI. Healthy
reopen is measured with an uncontrolled page cache; it is not a cold-device or
power-loss guarantee. Sampled disk allocation excludes filesystem metadata and
journals. The mixed test uses an injected storage-lane delay and separate quota
store, not actual disk exhaustion or sustained TLS fan-out.

The declared dedicated-device million-observation profile, elapsed 24-hour and
seven-day mixed runs, sustained peer lag/churn/rotation, device pressure,
crashed-host/power-loss trials and semantic engine migrations remain separate
release gates. See [CE-SW-40](../../../docs/software/CE-SW-40_operating_envelope_and_package_lifecycle.md)
and the [development plan](../../../docs/software/native_development_plan.md).
