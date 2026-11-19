# Packed history index trials

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
