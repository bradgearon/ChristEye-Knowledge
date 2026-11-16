---
title: "CE-SW-17 — Anaiero unified native command"
project: "ChristEye Anaiero"
status: "Implemented C++20 public module router"
version: "0.1.0"
date: "2026-11-16"
---

# Anaiero unified native CLI

**The public executable is `anaiero`, not `christeye`.** It routes commands to the already-existing native C++20 research components. Historical `ce_*` executables are internal modules, retained for compatibility with older scripts and CI.

Build and test:

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --target anaiero
ctest --test-dir build/native -C Release --output-on-failure
```

From the project root on Linux (or on Windows using `build/native/Release/anaiero.exe`):

```sh
./build/native/anaiero help
./build/native/anaiero doctor
./build/native/anaiero modules --json
./build/native/anaiero vm inspect examples/vlm/crest-garden.avlm
./build/native/anaiero process inspect examples/vlm/process-garden.avlm
./build/native/anaiero plan inspect examples/vlm/bread-first.cevplan examples/vlm/plan-garden.avlm .
./build/native/anaiero store verify ./durable-store
./build/native/anaiero colony step examples/connected-gardens.cev ./culture-state 5
./build/native/anaiero lab export examples/connected-gardens-lab.cev ./lab-state replay.ndjson
./build/native/anaiero fidelity evaluate examples/connected-gardens.cev ./culture-state examples/sim3/baseline.ceveval . fidelity-report.json
./build/native/anaiero select prepare examples/sim3/model-search.cevselect . ./selection-runs
```

## Router contract

| Public subcommand | Internal native executable | Purpose |
|---|---|---|
| `anaiero vm` | `ce_vlm` | Portable CEVLM/1 virtual machines |
| `anaiero process` | `ce_vlm_process` | Timed CEVLM/2 processes |
| `anaiero plan` | `ce_vlm_plan` | Crystal, learning, and RNA plans |
| `anaiero registry` | `ce_wind_registry` | Legacy digital evidence registry |
| `anaiero store` | `ce_wind_durable` | Durable WindStore and evidence cursors |
| `anaiero colony` | `ce_colony_runner` | Persistent synthetic multi-colony runner |
| `anaiero lab` | `ce_colony_lab` | Unified lab replay, overlays, and comparisons |
| `anaiero fidelity` | `ce_colony_fidelity` | Frozen synthetic model evaluation |
| `anaiero select` | `ce_colony_select` | Training-only selection and locked test |
| `anaiero sim` | `ce_living_sim_demo` | SIM1 digital demonstration |

`anaiero help [module]`, `anaiero version`, `anaiero doctor`, and `anaiero modules [--json]` are handled within the router. The JSON discovery schema is `anaiero.modules/1`, containing exact module names, internal backends, and installed availability.

The dispatcher resolves modules alongside its own executable and launches them directly using `execv` (Unix) or `CreateProcessW` (Windows). It does **not** interpolate commands into a shell, search arbitrary paths, or load plugins specified by a configuration file. Argument boundaries and exit status are forwarded. The trusted installation directory must remain protected from binary replacement; this is not a peer authorization or cryptographic-signing system.

The `anaiero` CMake target builds all runtime backends as dependencies, and `cmake --install` ships them into one `bin` directory. Existing `ce_*` scripts still work, but **new documentation and user-facing commands should use `anaiero`**. These are separate processes, not yet a single in-process shared-library runtime.

CI integration tests cover help, version, JSON modules, presence of backends, VLM1/VLM2/plan inspection, SIM1 invocation, a storage path containing spaces, rejection of arbitrary module names, and preservation of error exit codes.

All represented cell, RNA, learning, and molecular quantities remain synthetic or observational. The router provides no biological stimulation or culture-actuation capability.
