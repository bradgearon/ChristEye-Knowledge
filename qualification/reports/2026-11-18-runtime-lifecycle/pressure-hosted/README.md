# Hosted filesystem exhaustion

Both jobs exercised real filesystem exhaustion during admission and again
during publication. In each case reopening preserved the original committed
receipt and root. The interrupted write then succeeded once, its exact retry
returned the same receipt, and the canonical audit passed.

| Runner and isolated filesystem | Capacity | Bytes written by the filler in each phase | Native error | Checkout and result |
| --- | ---: | ---: | --- | --- |
| Ubuntu 24.04, tmpfs | 67,108,864 | 67,002,368 | ENOSPC (28) | PR merge 956dd3cc, source a8131574. The modular job passed. [Report](ubuntu-24.04/report.json) |
| Windows Server 2022, fixed VHD with NTFS | 65,990,656 | 53,542,912 | ERROR_DISK_FULL (112) | Push addf1fda. The VHD step passed. The overall job failed an earlier ordinary-workspace guard check on the marker bytes. [Report](windows-2022/report.json) |

The provenance files identify the job, checkout, SHA-256 digest of the
downloaded artifact ZIP, and each retained entry. Both reported results match
the job output. The Windows Release build manifest in the ZIP has CRLF line
endings. Its original bytes are retained alongside an LF-normalized copy whose
hash matches the compiled identity. These fixtures measure software recovery
after volume exhaustion. Physical media failure and power loss require
additional experiments. [Linux provenance](ubuntu-24.04/provenance.json) ·
[Windows provenance](windows-2022/provenance.json)
