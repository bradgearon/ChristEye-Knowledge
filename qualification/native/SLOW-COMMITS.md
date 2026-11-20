# Comparing throughput commit latency on one filesystem

Each synthetic qualification report retains the **32 longest timed throughput
commits** under `slow_commits`. The same timing sample drives the histogram,
exact threshold count and recorded commit duration. Each retained attempt
identifies its operation, expected generation, observation range, native
result, and whether the store published the commit. Per-attempt measurements
include lock wait, validation, staging, blob writes, transaction, ROOT,
index publication and cleanup. These disjoint intervals sum to the store's
measured total. The outer stopwatch also covers the call boundary.

Read `above_limit`, `above_limit_retained` and `above_limit_omitted`
together. A 100k run with 40 breaches keeps the slowest 32 and records eight
omitted breaches. Equal durations preserve the earliest attempts. Runs with
no breaches still show the longest successful attempts. Index work counts,
calling-thread CPU time and, on Linux where available, voluntary/involuntary
context switches come from the same attempt. A null resource value denotes
an unavailable counter. Disjoint stage times measure time spent in each
software phase, including time waiting for the scheduler or filesystem.

## Run the controlled pair

Use Python 3.11+ and the built native executable. Create the run parent on
the filesystem under evaluation and supply an exact-gate profile and device
declaration. Choose an order before starting the experiment.

```shell
python qualification/native/compare-commit-load.py /absolute/build/ce_native_qualification qualification/native/evidence-10k-exact.cevqualification /absolute/device.cevdevice /absolute/results/latency-pair --layout node-files --order idle-first
```

On Windows use the absolute path of the Release executable. The runner
executes the same binary twice on the chosen volume with byte-identical
inputs. It hashes the executable before and after each attempt, preserves
both native reports and stores, and refuses an existing output directory.
The conditions are named `idle` and `controlled-load`. An idle run leaves
the harness writer inactive. Declare any other writers, host scheduling
restrictions, mount and flush settings, filesystem, storage device and
firmware in the device declaration. Only claim an idle dedicated-device
result when the environment meets those conditions.

Both conditions start with a newly created 4 MiB sibling file filled with the
same deterministic blocks. Under controlled load one thread loops over that
file, overwriting and flushing 64 KiB at a time with a target start interval
of 20 ms. It never issues catch-up bursts. Each write alternates the contents
of the target block on successive passes. It begins before the native process
starts and stops after that process exits. The fixture removes its own file at
the end of each condition. The native store and its reports remain available.

`plan.json` pins the executable and input hashes, test order, layout, extent,
rate and flush policy before either native run starts. Each `condition.json`
records the actual writer's bytes, flush count, total/maximum flush latency,
period overruns, available volume space and timing. Linux snapshots also
record pressure totals from `/proc/pressure` and block device counters where
the filesystem exposes a corresponding `/sys/dev/block` device. These values
cover other activity on the host or volume. The snapshots use null where the
OS does not expose a measurement.

The result preserves both attempts even if the first native run fails a
latency gate. `comparable` requires two completed, audited attempts on the
same pinned build, profile and declared device. `all_qualification_gates_passed`
additionally requires that both native reports pass. The command returns
nonzero if either condition fails or cannot be verified.

Use a second new directory with `--order load-first` when the qualification
plan calls for a reversed pair. Later conditions may inherit a warmer device
cache and less free space because the previous store remains available.
Check the recorded free space and declared order alongside the latency data.
