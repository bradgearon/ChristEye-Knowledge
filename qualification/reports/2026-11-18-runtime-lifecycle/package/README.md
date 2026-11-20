# Two installed Linux releases: reviewed update and rollback

The [package-pair report](package-pair-report.json) records a completed local
GNU 14.2.0 Linux x86-64 Release run. Release A's 279 build-manifest source
entries match commit `a8131574e6e4024ea92ecd17e31a69aa0f59530c` byte for byte.
Release B was an independent compilation from that snapshot with one deliberate
change: the bounded display literal in `src/host/release_notice.txt`. Its C++
reader remained inside the verified state contract. Both packages were built
with the native controller enabled and optional mesh disabled.

The [fixture](../../../../native/tests/native_package_upgrade_tests.cmake)
installed both packages. It initialized the five-engine example, selected A,
and completed one cycle. The new host refused to open the retained session before
its cross-build binding was approved. Its reviewed activation then allowed B's actual host to finish
cycle 3; B's separately installed session CLI finished cycle 4 and audited
the store. Reversing the binding selected the earlier executable against that
same accepted suffix. Retrying the original update returned its receipt without
reselecting B. Release A completed cycle 5 and passed the final audit.

| Exact evidence | Release A | Release B |
|---|---|---|
| Installed envelope and executable inventory | [package-a-manifest.json](package-a-manifest.json) | [package-b-manifest.json](package-b-manifest.json) |
| Whole-build witness | [anaiero-builtin-manifest-a.txt](anaiero-builtin-manifest-a.txt) | [anaiero-builtin-manifest-b.txt](anaiero-builtin-manifest-b.txt) |
| State-contract witness | [anaiero-compatibility-manifest-a.txt](anaiero-compatibility-manifest-a.txt) | [anaiero-compatibility-manifest-b.txt](anaiero-compatibility-manifest-b.txt) |

The exact build and executable digests differ. The 130-entry state witnesses
are byte-identical; their shared SHA-256 is
`d94e3f200c052c1076ec1f531ac9756f7ce460fab97950db7cd12a25ce0a0f90`.
The [provenance record](provenance.json) includes byte lengths and SHA-256s
for all raw reports and manifests. Every native source digest was checked
against its fixture snapshot, and both inventoried host hashes were checked
against the installed executables before collecting these files.

The [separate integrated CTest run](deployment-ctest.normalized.log) passed the
deployment, compatibility and installed-package suites (3/3; 9.46 s). It
covers interrupted canonical publication/retry, missing or unpinned build
witnesses, unsupported precontract/codec transitions and bounded witness
retirement. The package pair is a test release with unchanged engine/state
semantics, not a published second product release, a general schema
migration, a long-duration qualification or a power-loss experiment. It did
not install an OS service; actual SCM and systemd operations require their own
isolated hosted reports. These results describe the stated source revision,
not later Windows compiler-warning fixes.
