# Packed history index trials

## Frozen source trials

Source `97b20a1f` ran the original 10,000-observation commit profile twice on
one shared Linux overlay. Both runs passed with 0 of 159 commits over two
seconds. The slowest baseline commit took 469.84 ms. The slowest loaded
commit took 445.94 ms while the competing writer completed 4,404 flushes.
The pair did not reproduce the earlier four overruns. The host and its other
workloads were uncontrolled. [Paired run and per-commit records](commit-load-10k/README.md)

On hosted Ubuntu and Windows, the four fresh 256-observation stores ran in
single-node, packed, packed, single-node order. Both pairs on each platform
passed the native and comparison gates. Each packed store used 1,378 fewer
regular files after compaction and 2,824 fewer regular files plus directories
at the verified replacement checkpoint, while both indexes were present.
[Ubuntu](hosted/ubuntu/smoke/comparison.json) · [Windows](hosted/windows/smoke/comparison.json)

## Development pilot

The [development smoke comparison](development-smoke/comparison.json) ran four
fresh stores from one Linux executable in single-node, packed, packed,
single-node order. Both pairs passed all native gates. At 256 retained
observations each single-node store occupied 1,718 regular files after
compaction. Each packed store occupied 340. While both indexes coexisted,
the packed trial used 2,824 fewer regular files plus directories in each pair.
Neither layout had a commit over the two-second limit in this short run.

The reports identify the exact binary, build and profile. The executable was
built from the development tree while other checks were running on the shared
Linux filesystem. This short comparison exercises the protocol and records
its measured file cost. Review the larger trials for latency and capacity at
their respective sizes.

[Experiment and operator commands](../../native/PACKED-INDEX.md)
· [Architecture](../../../docs/software/CE-SW-42_packed_index_and_latency_experiments.md)
