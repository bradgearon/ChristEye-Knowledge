# Controlled commit-load pair on shared Linux

Source commit `97b20a1f`. Both conditions ran the same instrumented executable, exact 10,000-observation profile
and operator declaration in new stores on the same overlay. The baseline ran first.
The load worker rewrote a bounded 4 MiB sibling file in 64 KiB blocks and called
fsync after each write, targeting 20 ms between writes. Other host activity and
the device cache were uncontrolled. These runs measure one pair on this shared
filesystem.

| Condition | Native gates | Commits over 2 s | Longest commit, ms | Controlled flushes during invocation | Total run, s |
|---|---|---:|---:|---:|---:|
| idle | Passed | 0/159 | 469.84 | 0 | 89.19 |
| controlled-load | Passed | 0/159 | 445.94 | 4,404 | 89.82 |

The [predeclared plan](plan.json) fixes the order and hashes. The [paired result](comparison.json)
records actual writer activity and filesystem/host snapshots. The
[baseline](idle/native/report.json) and [loaded](controlled-load/native/report.json)
reports retain the 32 longest attempts with their individual stage times, index work
and thread activity. [Provenance](provenance.json) lists exact artifact hashes
and the verified native source tree. The earlier frozen 10k failure remains a
separate measurement.
