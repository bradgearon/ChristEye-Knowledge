# Short five-engine host and TLS qualification

These Linux Release diagnostics exercised the working tree based on
`d742d556b636cf21cdef34898fc30e5fff9c0729`. The local compiler was GCC 14.2.0.
The two executables were compiled at different points during integration:
each directory contains the exact build manifest whose SHA-256 appears in its report.
Neither run is attributed to a later clean commit or a particular storage device.

| Exercise | Result | Evidence |
| --- | --- | --- |
| Ordinary campaign | All 32 cycles, finite plan, seven p99 gates and final audit passed; 122 engine-step observations. | [Report](campaign/normal/report.json) |
| Process exit after cycle 10 committed, before campaign progress was checkpointed | Resume reconciled the canonical work. All 32 cycles and audit completed on attempt 2; continuous qualification correctly failed. The resumed interval also had no durable control timing sample. | [First attempt](campaign/work-cut/attempt-1-interrupted.json), [resumed report](campaign/work-cut/report.json) |
| Process exit after one engine committed, before the pending cycle completed | The next attempt completed the pending cycle and remaining work once, preserved the finite plan, and passed the audit. The uninterrupted-duration claim correctly stayed false. | [First attempt](campaign/engine-cut/attempt-1-interrupted.json), [resumed report](campaign/engine-cut/report.json) |
| Process exit after final canonical report, before publication | Resume republished the exact attempt-1 completion bytes; original short-run acceptance and 122 observations were preserved. The test also verified repeated resume is read-only on completed canonical state. | [Report](campaign/publication-cut/report.json), [CTest log](campaign/ctest.normalized.log) |
| Real TLS with slow/offline peers, replay, certificate rotation and revocation, churn and retention | 32 cycles, remote cursor 32, eight retired empty source epochs and 124 observations at each local host phase. Twenty-eight successful page acknowledgements had an exact nearest-rank p99 of 12.215320342 s (60 s diagnostic ceiling). Old and revoked credentials were rejected; those rejections never entered the latency distribution. | [Report](tls/mixed-mesh-report.json), [CTest log](tls/ctest.normalized.log) |
| Guard against filling a shared workspace | The marked directory on the normal volume was rejected before a filler, store or report could be created. This run did not exhaust a volume. | [CTest log](pressure-guard/ctest.normalized.log) |

The [earlier failing TLS test](tls-prior/ctest.normalized.log) is retained: its
fixture incorrectly tried to renew a generation-1 subscription after replacing
the credential. The passing fixture explicitly obtains a generation-2
subscription. The original rejection was not turned into a successful latency sample.

The campaign and TLS directories contain their original profiles, session input
and compiled session definition. The four campaign cases used byte-identical
inputs, saved once under `campaign/`. Successful campaign timing distributions
cover only the reported attempt; prior attempt files preserve measured
checkpoints, without crediting unmeasured work or downtime. No 24-hour,
seven-day, Windows, physical power-loss or volume-exhaustion result follows
from these short runs. TLS latency measures offered-page write through canonical
acceptance of the matching ACK; it includes the deliberately delayed receiver.

[Provenance](provenance.json) lists byte counts and SHA-256 digests and
describes path normalization in the CTest logs. The original binary reports,
profiles and session definitions were copied byte-for-byte. No certificates,
private keys, executables or full stores are included.
