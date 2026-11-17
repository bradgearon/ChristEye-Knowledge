# Anaiero native WindBus SW1 prototype

## Public executable: anaiero

**Native host parity:** Anaiero's planned lightweight long-running controller is a **native Windows and native Linux** application with one portable C++20 core. Both platforms are first-class, using IOCP/named pipes/SCM or epoll/Unix-domain sockets/systemd in infrastructure adapters. It will not require WSL, containers or a scripting runtime. Today's CLI/tools already build on Windows and Linux; full supervised host adapters are planned, not yet delivered. See [ADR-002](../docs/software/ADR-002_native_windows_linux_parity.md).

Use **`anaiero`** as the single public native command. The old `ce_*` programs remain as internal backends for compatibility, but new user scripts should call `anaiero`:

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --target anaiero
./build/native/anaiero help
./build/native/anaiero doctor
./build/native/anaiero vm inspect examples/vlm/crest-garden.avlm
./build/native/anaiero colony step examples/connected-gardens.cev ./culture-state 5
./build/native/anaiero store verify ./durable-store
./build/native/anaiero select prepare examples/sim3/model-search.cevselect . ./selection-runs
```

On Windows, the executable is generally under `build/native/Release/anaiero.exe`.
See [CE-SW-17](../docs/software/CE-SW-17_anaiero_unified_native_cli.md).

**Status: digital-only portable C++20 reference implementation; not a security/biological-control release.**

This directory implements the initial **SW1** scope from [CE-SW-01](../docs/software/CE-SW-01_native_mesh_architecture.md):

- Header-only, allocation-free **WindPacket/1** encoder and decoder. Exact 56-byte little-endian header; no native-struct casting. Bounds, schema family kind, delivery class, flags, versions and TLV framing are validated. Required unknown TLVs and duplicate singleton TLVs are rejected. Payload remains opaque.
- Bounded, single-owner **weighted endpoint scheduler**, with five physically separated capacity pools, admission status, expiry receipts to the caller and wake-on-empty-to-nonempty callback. Weights 8:6:4:3:1 model *processing share*, never scientific confidence or an actuation permit.
- Deterministic tests for wire offsets, round-trips, invalid/truncated packets, unknown required TLVs, duplicate TLVs, queue overflow, expiry, fairness and randomized malformed input; an event-triggered in-process loopback demonstration.

Build (Linux/macOS/Windows with CMake 3.20+ and C++20 compiler):

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

### What this code does *not* do

- **No network transport**: the codec is not a QUIC, Noise or TLS implementation. A packet flag never authenticates a peer. Future adapters must verify the secure session, ACL, source-slot mapping, schema and consent scope before accepting records.
- **No durable delivery**: `accepted` means an in-memory queue slot, not persisted, network acknowledged, delivered or biologically acted upon. Future SW2 adds WindStore/WindChange durability.
- **No direct sensor / neural / actuator driver**: only opaque packet bytes and caller-owned work tokens. No chemical, molecular, stimulation or pathogen operations are present.
- **No multithreaded queue claim**: the scheduler is explicitly single-owner; platform producers serialize into it before calling `post`. An ISR must only signal a task / DMA descriptor and must not parse untrusted packets.
- **No background polling**: a platform callback or notified task posts work, and the recipient drains `next()` until empty. Periodic status timers are not transport delivery loops.
- **No time-domain conversion**: deadlines use the caller-provided monotonic clock domain. Clock anchors, uncertainty, persistence and provenance are future integration requirements.

### SW2 and SW3 follow-ups

Implement a native read-only ledger, durable receipt state machine, hash-addressed immutable objects, synchronized connection handshake, authenticated identity and replay protection, with CPU/MCU performance tests and formal wire compatibility vectors. The Python reference remains supported during migration. This component has no proven worst-case interrupt or network latency and has not yet been compiled for a specific MCU toolchain.

## SW1.1 hardening (this branch)

- WindPacket encoding detects input/output buffer overlap and returns `overlapping_buffers` before modifying the output frame. Its decoded `View` remains a **borrowed**, synchronous reference; callers must keep the underlying input bytes alive until consumed or copy an owned frame into their bounded storage before asynchronous dispatch.
- Fault notifications are dispatched ahead of weighted digital work. The other four queues use *smooth* weighted round robin (6:4:3:1), avoiding long consecutive bursts from grouped slot scheduling. This is **item-weighted**, not byte-cost fair scheduling; reliable byte-credit/bandwidth admission remains a future layer. Independent hardware A22 limits never depend on queue dispatch.
- Callbacks and posting remain **single-owner**: real multithreaded/ISR producers need a separately synchronized bounded ingress ring. No raw ISR directly parses untrusted frames.

## AVM1 virtual PCM and MIDI patchbay

`include/christeye/wind/media_patchbay.hpp` adds a freestanding-compatible, fixed-capacity, digital-only software routing core. Ports have stable numeric IDs, PCM/MIDI media kind, producer or consumer role, exact clock domain/sample-rate/channel matching and an upstream `verified` flag. Route commits require an authorized caller and matching expected revision. The patchbay synchronously fans out **borrowed** PCM blocks (interleaved float samples) or UMP word arrays to pre-registered callbacks. Hot-unplug invalidates affected routes and increments muted-route receipts.

It does **not** implement operating-system drivers, ringbuffer scheduling, converter/sample-rate conversion, real MIDI-CI device discovery, audio mixing, cryptographic verification, physical audio I/O, network media, neuronal speech, or biological actuation. Integrators must validate `verified` using actual signed installed-hardware manifests/ACLs; a user-provided boolean is not authentication. The callback is synchronous and must be nonblocking; the samples are invalid after the callback returns unless copied by the consumer.

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

Additional `wind_media_tests` verify two-route audio fan-out, typed MIDI delivery, mismatched clocks/rates, malformed frames, stale route revision, unauthorized route, queue capacity and removal-induced muting. No timing benchmark or device validation is claimed.

See [CE-SW-05](../docs/software/CE-SW-05_modular_audio_vision_midi_starchip.md) for the hardware-model and research-safety contract.

## SIM1 — deterministic living-system research surrogate

SIM1 implements a **synthetic digital simulator**, not a biologically validated life model. See [CE-SW-06](../docs/software/CE-SW-06_living_surrogate_instruction_simulator.md). It represents symbolic crest and neural progenitors, blood carriers, neuron/glia archetypes, runtime-defined molecular proxy classes, bounded 2D space, synthetic energy/growth/RNA/learning counters, and directed graph connections.

The fixed-capacity C++20 core lives in `include/christeye/living/simulator.hpp` and the 32-byte little-endian instruction codec (CEIP/1) in `include/christeye/living/bytecode.hpp`. SPAWN, DEFINE_MOLECULE, SUPPLY, MOVE, DIFFERENTIATE, CONNECT, TEACH_SYMBOL, ADVANCE and OBSERVE are deterministic and explicitly reject invalid transitions. The simulator owns its journal, copyable checkpoints and FNV consistency hashes (**not cryptographic**).

`tests/living_sim_tests.cpp` verifies replay, bytecode, illegal lineages, bounded capacities and the synthetic **observation → WindPacket encode → decode → weighted scheduler** route. The demo prints ten NDJSON frames of a moving synthetic carrier:

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
./build/native/ce_living_sim_demo
```

Each frame is tagged `SIM1_UNCALIBRATED_DIGITAL`. This work does **not** demonstrate an organoid, biological RNA motion, neuronal language learning, viability, a physical Crystal or experimental material delivery. Real instrument links, signed provenance and durable WindStore remain separate future work.

## VLM1 — portable virtual living machines

See [CE-SW-07](../docs/software/CE-SW-07_virtual_living_machine_bundles.md). A single editable `.avlm` image includes initial synthetic cells/molecules/Soil; a sealed instance adds the deterministic runtime journal and FNV consistency checks (not cryptographic). The host-only `virtual_machine.hpp` wrapper supports 8×8 six-channel conservation-based synthetic field movement, independent image cloning, full-state forks and deterministic replay. It does not control real cultures or prove neural learning.

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
# adjust executable path for your platform
./build/native/ce_vlm inspect examples/vlm/crest-garden.avlm
./build/native/ce_vlm new examples/vlm/crest-garden.avlm colony-a.avlm colony-a
./build/native/ce_vlm run colony-a.avlm colony-a-25.avlm 25
./build/native/ce_vlm verify colony-a-25.avlm
./build/native/ce_vlm fork colony-a-25.avlm colony-b.avlm colony-b
./build/native/ce_vlm execute colony-b.avlm examples/vlm/teacher-session.ops colony-b-next.avlm
./build/native/ce_vlm stream colony-b-next.avlm 10
```

The `stream` output is NDJSON with all synthetic actors, proxy counters and a six-field 8×8 grid per tick, usable by a future visualization. Physics, biology, instruction schedules and constants are arbitrary software assumptions. No automatic stimulation, molecular delivery, physical Crystal learning or operator-free culture intervention is present.

## SW2 reference — WindRegistry

The host-only `ce_wind_registry` CLI and `christeye/living/wind_registry.hpp` provide immutable SHA-256 object identity, a replay-checked append-only evidence journal, per-entity and per-kind pushed subscriptions, stage-to-Crystal/RNA/support mapping and immutable successor proposals. See [CE-SW-10](../docs/software/CE-SW-10_windstore_learning_crystal_registry.md). This is **not** crash-safe production WindStore (no fsync/locking), nor an authenticated multi-host transport or biological action pipeline.

## SW4 — `anaiero runtime` latest-state projector

The native controller path is `SIM3 -> typed WindPacket/1 -> bounded weighted queue -> ColonyState projector -> post-commit subscriptions`.
Run `anaiero runtime simulate examples/connected-gardens.cev ./culture-state 5` or start the blocking local event shell with `anaiero runtime serve examples/connected-gardens.cev ./culture-state` and issue `STEP`, `STATE`, `QUIT`.
The in-RAM bounded latest-state snapshot and individual source cursors are not replacements for authenticated mesh joining, qualified laboratory instruments, or independently managed physical care. See [CE-SW-18](../docs/software/CE-SW-18_colony_state_runtime_projector.md).

## SW5 — digital cell-behavior atlas and development

Use `anaiero cells` as the public CLI. The sealed `CEVCELLATLAS/1` declares bounded, dimensionless illustrative behavior parameters for nine cell archetypes. Its `CEVCELLSOURCE/1` companion records evidence identity; `CEVCELLRUN/1` stages simulated development and learning. Checkpoints (`CEVCELLSTATE/1`) preserve lineage and synthetic network state. See [CE-SW-19](../docs/software/CE-SW-19_cell_behavior_atlas_and_developmental_surrogate.md).

```sh
anaiero cells sample examples/cells/crest-blood-neural.cevatlas ./example-culture.cevcells
anaiero cells execute examples/cells/crest-blood-neural.cevatlas ./example-culture.cevcells examples/cells/reference-development.cevcellrun
anaiero cells inspect examples/cells/crest-blood-neural.cevatlas ./example-culture.cevcells
```

This is a **synthetic-only simulation**. It does not model culture conditions, biological stimulation, pathogen effects, gene engineering, or proven neural knowledge transfer. The controller's qualified live-observation and physical-care boundaries remain separate.

## Adaptive Optimization (v0.015)

`anaiero optimize demo` runs a synthetic digital route-selection/feedback example via a service, a trial port and an in-memory digital route store. ColonyState remains only the read model. Extensibility hooks are defined by `IServiceExtension` and opt-in service-specific strategy ports. No real cells or physical actuators are controlled. See [CE-SW-21](../docs/software/CE-SW-21_adaptive_optimization_and_service_extensions.md).

## SW5 forked optimization and durable digital route policy

`anaiero optimize fork demo examples/cells/crest-blood-neural.cevatlas ./digital-forks` evaluates three real SW5 checkpoint forks, selects a route using guarded multi-objective scoring and stores digital policy state plus a pending outbox receipt behind a SHA-256-pinned atomic HEAD. Reopening reads the existing committed state without duplicating its route. `ce_fork_optimizer_tests` exercises object/HEAD fault injection, receipt replay, time-preserving rollback and invalid HEAD. No living-cell actuation or authenticated outbound delivery is implemented. See [CE-SW-22](../docs/software/CE-SW-22_forked_simulation_optimization_and_durable_routes.md).

## SW6.1 — Durable route outbox to WindBus and projector

`anaiero optimize fork sync <atlas> <route-session-dir> <windstore-dir> <session-id> <source-slot>`
replays verified route receipt packets from WindStore, projects pending digital
route status into the volatile ColonyState view, and acknowledges the source
route ledger only after that state is readable. The source slot is a local
configuration value, **not** an authenticated peer identity. No real-cell
controls or external mesh acknowledgement. See [CE-SW-23](../docs/software/CE-SW-23_durable_route_bus_projection.md).

## CE-SW-24 — Science bridge and hypothesis runtime

The new public command `anaiero science` runs a small **digital-only**
SW5 neuron/astrocyte experiment with four software conditions, a bounded
fast/slow coordinator, read-only model trace adapters and a WindBus ->
ColonyState demonstration:

```sh
anaiero science engines
anaiero science compare examples/cells/crest-blood-neural.cevatlas
anaiero science bus examples/cells/crest-blood-neural.cevatlas
anaiero science trace examples/scientific/native-reference.cevscitrace
```

NEURON and BRAINCELL exported neuronal model outputs, and PhysiCell/MOOSE/STEPS
population/chemical model outputs, are supported **only through optional offline
normalized trace exchange** at this stage. Anaiero does not install, link,
or claim to execute these solvers. Optional NEURON and CSV converters live
under `tools/scientific/`; Python is not required by the native C++ controller.
All normalized values are computational proxies, not physiological readings.
See [CE-SW-24](../docs/software/CE-SW-24_scientific_simulation_foundation.md).

## SW6.2 source epochs and SW7.1 native live scientific coupling

**General observation replay:** `anaiero science epoch-demo <atlas> <windstore-dir>`
records two simulation producers, restarts one producer under a second source
epoch, and rebuilds the same ColonyState view from the committed WindStore
event stream after restart. The v1 packet remains available; new epoch-scoped
readings use schema `0xCE600041` and cannot silently lose their epoch.

**Live numerical-model stepping:** the C++20
`anaiero science live <atlas> <windstore-dir> <epoch> <model-sha256> synthetic`
command exchanges bounded `STEP`/`SAMPLE` frames over stdin/stdout with an
optional external model. A Python driver is included solely as an optional
test/model sidecar; Anaiero itself needs no Python or NEURON installation.

```sh
python tools/scientific/live_solver_bridge.py \
  --anaiero ./build/native/anaiero \
  --atlas examples/cells/crest-blood-neural.cevatlas \
  --store ./live-evidence --epoch 1 --backend synthetic

# Optional when NEURON is separately installed in this Python environment:
python tools/scientific/live_solver_bridge.py \
  --anaiero ./build/native/anaiero \
  --atlas examples/cells/crest-blood-neural.cevatlas \
  --store ./live-evidence --epoch 2 --backend neuron
```

On Windows pass the native path to `anaiero.exe`. The current prototype uses
a read-only *digital* NEURON model sidecar when present; it is not a physiological
instrument or a validated live-cell interface. See
[CE-SW-25](../docs/software/CE-SW-25_source_epoch_recovery.md) and
[CE-SW-26](../docs/software/CE-SW-26_live_multirate_solver_coupling.md).

## CE-SW-27 — Reusable simulation kernel

The public native `anaiero kernel demo <atlas.cevatlas> <sim3-config.cev> <run-dir>`
composes the existing SW5 CellBehaviorService and SIM3 durable Runner using
a versioned `ISimulationEngine` lifecycle. Engine-specific state remains
private. SW5 supports CEVCELLSTATE checkpoint and restore; SIM3 retains its
separate persistence model, and arbitrary in-place rewind is explicitly
unsupported. See [CE-SW-27](../docs/software/CE-SW-27_reusable_simulation_kernel.md).

## Native local controller (experimental)

Native Windows and Linux `anaiero runtime start <endpoint> <sim3.cev> <state-dir>`
opens the existing durable SIM3 digital colony and hosts an authenticated local
control endpoint. Linux uses same-UID Unix-domain sockets; Windows uses named
pipes and verified user SIDs. `anaiero runtime query <endpoint> PING|STATE|STEP|STOP`
is a separate native CLI process. STEP advances **only the digital simulator**.
The endpoint does not accept biological care, feeding, stimulation, or arbitrary
module loading commands.

This is a *console/headless process*, not yet registered as an SCM service or
systemd service, and not a secured remote mesh. Disk sync and subprocess
timeout supervision require further hardening. See
[CE-SW-28](../docs/software/CE-SW-28_native_controller_host_and_queue.md).
