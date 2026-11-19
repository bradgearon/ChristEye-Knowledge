# Actual systemd upgrade and rollback on Ubuntu 24.04

[GitHub Actions job 135315964263](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451398830/job/135315964263)
completed both the two-package build and the installed systemd lifecycle
steps. This was the pull-request run for source head
`a8131574e6e4024ea92ecd17e31a69aa0f59530c`; the checkout recorded in the
job log was its test merge `956dd3cc16c5585b9ccc2802a00494213e2c733d`.

The [native pair](package-pair-report.json) used two actual installed hosts,
with distinct build and executable digests and one compatible state contract.
It recorded four cycles before rollback; the earlier executable continued
through five and audited the store. In a separate store the
[systemd fixture](supervisor-report.json) installed the first release stopped,
started and drained it at 15 completed cycles, then deliberately committed a
new selection before updating the registration. Starting that stale
registration failed. After reconciliation the second executable accepted
work through cycle 30; the reverse switch retained all 30. Removing the
service left the packages and canonical state available for a successful audit.

All eight files in [artifact 14496151965](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451398830/artifacts/14496151965)
are preserved here as exact bytes. The downloaded archive SHA-256 matched
the digest emitted by GitHub, and the reported build, host and state
identities matched their corresponding package inventories. The
[provenance record](provenance.json) lists each file's SHA-256 and byte count.
This short Linux fixture establishes software activation, interrupted
registration recovery and rollback for this actual release pair. It does not
cover Windows SCM, uninterrupted long-duration operation or physical power loss.
