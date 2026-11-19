# Hosted Windows 100,000-observation qualification

This is the completed **failed** Windows qualification for PR #53 head
`c196e56ba71428de4d568217c542bf2f635cb005`. The exact report JSON was captured
from the native executable's output in the [job log](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44396960675/job/135129339986).
The [original CI artifact](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44396960675/artifacts/14472567230)
retains the report, exact profile, build manifest and regression logs.

The workload completed on Windows Server / NTFS: 100,000 throughput observations,
100 lifecycle observations and 100 retired epochs, with at most one active
lifecycle source. All recovery identity, full audit, indexed read, memory and
disk gates passed. The only violation was the unchanged batch-commit p99
histogram upper-bound budget: **2.147483648 seconds against 2 seconds**.
The maximum observed batch commit was 2.3158684 seconds. The report records
1,588 batch samples; the p99 field is a conservative histogram bound, not an
exact interpolated percentile.

The profile SHA-256 is
`40b24cc47571b3b348a88106943d8657c4ad5bd4ab9522182243998f8accb76e`.
The compiled build identity is
`b4553c3b0b6019bca435ded12cabab746bec46ed03a4a13aa1035e63a040a8c6`.
The hosted job built the pull-request merge ref; the report's build identity
and original manifest are the precise compiled-source witnesses.

This negative result remains independent of later optimization measurements.
Neither its report nor its threshold is rewritten after the outcome. It does
not establish a general NTFS latency limit or a cause for the measured tail.

The original [CI archive](artifact.zip), [raw build manifest](build-manifest.txt),
and [provenance record](provenance.json) are now retained beside this report.
The report and profile were compared with the original archive and match exactly.
The actual PR merge checkout was
`5cc720db4e88024c1705db7228c41406ae36944b`.
Windows wrote the manifest with CRLF bytes; the separately labeled
[LF manifest witness](build-manifest.lf.txt) preserves the in-memory representation
hashed by CMake, whose SHA-256 matches the compiled build identity above.
The raw file is unchanged, and both hashes are recorded.

Later passing and failing measurements, including the explicitly versioned
percentile estimator, are indexed in the
[operating-envelope evidence](../2026-11-18-operating-envelope/README.md).
