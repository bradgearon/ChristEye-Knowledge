# Windows package build: rejected before migration ran

[Hosted job 135315963472](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451398830/job/135315963472)
for source commit `a8131574e6e4024ea92ecd17e31a69aa0f59530c` failed while
building release A. MSVC reported C4456 for inner variables named `previous`
in `deployment.cpp` and `expected` in `native_deployment.cpp`; warnings
are errors for this build. The fixture never created a verified Windows pair
or exercised the Windows supervisor. Linux results do not replace that gate.

The correction gives each role a distinct local name and preserves the
comparison and receipt logic. The mixed runner's completion callbacks use
the same convention. No failure is reclassified by the correction.

## Separate Windows storage fixture

The [corrected-source push job](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451985553/job/135317892969)
built the native distribution. Its ordinary-workspace guard stopped at the
ownership check instead of reaching the volume check: the canonical marker
uses LF bytes, while the test setup used platform text output. The harness
now writes explicit LF and checks the bytes before invoking the unmodified
native guard. Its separate, real NTFS virtual-disk exhaustion step passed.
These are different experiments. This job did not run the synthetic 100k
profile because its preceding CTest step failed.
