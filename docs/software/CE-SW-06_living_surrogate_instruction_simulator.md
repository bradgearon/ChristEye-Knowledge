---
title: "CE-SW-06 — Native Living-System Digital Surrogate and Instruction Machine"
project: "ChristEye Anaiero"
revision: "SIM1 0.1"
classification: "Digital simulation, evidence contracts and synthetic integration testing"
status: "Executable synthetic reference; NOT a validated biological digital twin"
related: "CE-SW-01..05, BIO-01/BIO-02/BIO-03/BIO-04/BIO-06, A09/A16/A22"
---

# CE-SW-06 — SIM1: Living-system surrogate and instruction machine

## Scientific meaning of “identical”

It is not currently possible to implement a simulation that **behaves identically to all real living components** of Anaiero. The biological device, its neuron-level transfer function, and the proposed physical Crystal/word mechanisms have not been observed and validated. Such a claim would be circular: matching a software model to rules chosen by the designer proves only that the implementation follows those rules. SIM1 is a **digital surrogate** that satisfies testable software invariants, exposes assumptions, and can be calibrated against independently acquired data later. It does not predict life, make nutrient recipes, demonstrate language in neurons, or prove physical memory transfer.

SIM1 enables the technical work that *can* be tested today: a deterministic instruction set, correct lineage boundaries, time-varying moving actors, bounded molecules and connection data, reproducible state replay, and documented WindBus publication.

## Decomposition: cells, molecules, graph and Soil

The header-only C++20 simulator is under `native/include/christeye/living/simulator.hpp`. It requires no heap, threads, clock syscalls, operating-system facilities or biological drivers. Every Simulator instance owns a bounded region of cell slots, molecule slots, directed connection edges and provenance events; fixed capacities are compile-time template arguments suitable for microcontroller or host builds. It is a single-owner value type: copy it to create a complete checkpoint.

- **Cell** (`id`, `parent_id`, `kind`, synthetic `x,y`, `energy`, `maturity`, `neurite_proxy`, `rna_proxy`, `learning_proxy`, `last_symbol`, `viable`). Nine symbolic archetypes: crest progenitor, blood carrier, neural progenitor, peripheral neuron, central neuron, astrocyte, Schwann, microglia and endothelial cell. “Neurite proxy” is not a measured axon; “RNA proxy” is not an RNA assay.
- **Molecule** (`id`, abstract class, dimensionless quantity, loss-per-step). Classes support, guidance, transport, activity, RNA, protein and waste *proxies*; IDs are developer-defined per scenario. There are no peptide sequences, wet-lab formulations, concentrations or kinetics.
- **Connection** (`source`, `target`, `weight`). A synthetic graph edge; it is not a physical synapse or proof of a Root.
- **Soil** is the bounded two-dimensional abstract coordinate domain 0–4096 with a shared abstract inventory. This first release has no physical diffusion, fluid dynamics, extracellular matrix model or real neuronal spike integration.
- **Evidence** (`sequence`, simulated tick, opcode, IDs, state hash, parent chain hash, model revision). The journal records every accepted mutation; rejected instructions do not change state or append events.

Biological lineage matters: crest progenitor may transition to a candidate **peripheral** neuron or Schwann archetype, and neural progenitor to central neuron or astrocyte archetype in the illustrative rule set. Blood carriers are not converted into neurons or glial cells. These type constraints do not claim an actual differentiation protocol. An externally seeded central neuron is a **scenario fixture**, not something produced from a blood cell.

## Fixed 32-byte instruction set (CEIP/1)

`native/include/christeye/living/bytecode.hpp` encodes commands in a fixed little-endian bytecode, no raw C++ struct serialization:

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 4 | Magic `CEIP` |
| 4 | 1 | Major version `1` |
| 5 | 1 | Opcode |
| 6 | 2 | Reserved, zero |
| 8 | 4 | Subject ID |
| 12 | 4 | Target ID or archetype |
| 16 | 4 | Signed value |
| 20 | 4 | Signed extra value |
| 24 | 4 | Parent cell ID |
| 28 | 4 | Reserved, zero |

| Opcode | Semantics | Restrictions |
|---|---|---|
| `SPAWN` | Add candidate cell archetype and position | Unique ID, legal parent/type boundary, bounded position |
| `DEFINE_MOLECULE` | Register named-by-ID symbolic molecular class and initial inventory | Unique ID and bounded nonnegative values |
| `SUPPLY` | Add/subtract symbolic inventory | No physical medium or chemical delivery |
| `MOVE` | Move synthetic actor on 2D coordinates | World bounds; no physical manipulation |
| `DIFFERENTIATE` | Apply permitted symbolic lineage transition | No blood-to-neuron shortcut |
| `CONNECT` | Create directed synthetic neuron-neuron edge | Existing viable neuron archetypes only |
| `TEACH_SYMBOL` | Update an artificial familiarity counter | Not language understanding or an engram |
| `ADVANCE` | Execute N bounded synthetic clock steps | N in [1,10000], no wall clock |
| `OBSERVE` | Append a non-mutating evidence checkpoint | Not a sensor measurement |

Wire decoding checks exact frame size, magic, version, opcode and reserved fields; malformed programs fail. Execution is **transactional per instruction** rather than whole-program atomic: a failure at instruction k leaves earlier successful instructions and their evidence records committed. The caller can checkpoint/restore for all-or-nothing workflows. Network authentication, authorization and canonical cryptographic record hashing are handled by future WindBus/WindStore services, not by `CEIP` itself.

## Declared (arbitrary) synthetic dynamics

The rules are deliberately simple and synthetic, not a biologically calibrated neural or metabolic model:

- each accepted `ADVANCE` increments simulated time by exactly one integer tick per substep;
- molecular inventory decreases by its symbolic loss-per-tick;
- viable actors experience abstract energy cost and support-dependent gain; no culture medium is consumed or dispensed;
- cell maturity increments by a symbolic step value;
- neuron archetypes increment neurite/RNA proxies under active abstract guidance/RNA inventory;
- blood-carrier actor moves one synthetic x-coordinate per tick, wrapping at the world boundary;
- configured synthetic connection weights increase one unit per supported tick (up to cap).

For example `E[t+1] = clamp(E[t] + 2*I(support>0) - cell_cost, 0, 1000)` is an **algorithmic simulation rule**, not a physiological law. Different values or rules must create a new model revision before comparing evidence. SIM1 does not conflate success on a software classifier with stored neural semantics.

## Real-time bus integration

The simulator calls a nonblocking **evidence callback** on accepted commands. `native/tests/living_sim_tests.cpp` connects that callback to the existing `WindPacket/1` serializer and `BoundedScheduler` evidence lane, decodes each frame, and verifies ordered delivery. This is synchronous in-process integration; it is not a secure distributed transport or durable WindStore receipt.

Each model event may become a future `SIMULATION_OBSERVATION` typed WindBus record with `model_id`, `model_revision`, `scenario_digest`, `instruction_sequence`, `synthetic_tick`, `snapshot_digest`, `parent_event_digest`, and `measurement_class=SIMULATED`. Keep simulated facts physically separate from authenticated real sensor observations. The current FNV-1a state hash only checks deterministic consistency; it is **not a cryptographic signature or tamper-evident hash**. Production provenance will use canonical cryptographic content hashes and keyed identities.

## Running and watching the system move

```bash
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
./build/native/ce_living_sim_demo
```

The example seeds synthetic neural-crest, blood-carrier and neural-progenitor participants, declares support/RNA proxies, adds a neuron-neuron edge, records a teaching symbol, and runs ten one-step instructions. It prints **ten newline-delimited JSON frames** containing positions, counts, synthetic RNA/neurite proxies, energy, source tick, evidence count and state digest. Blood-carrier position and other proxy values change between frames, giving a machine-readable feed for a future Canvas/Pages 2D replay viewer. The demo outputs `SIM1_UNCALIBRATED_DIGITAL` in every frame.

## Integration and acceptance gates

- **SIM1.A — deterministic replay:** same instruction stream must create byte-for-byte equivalent logical state, event chain, movement and metrics on repeated runs and across supported compilers.
- **SIM1.B — reject invalid changes:** duplicate IDs, illegal lineage transitions, invalid molecule classes, illegal connections, overflowing fields and out-of-world movements must not mutate state.
- **SIM1.C — bounded execution:** full journals/molecule slots/cell slots reject further work without heap growth.
- **SIM1.D — instruction transport:** CEIP/1 encode/decode and corrupt-stream rejection before any execution.
- **SIM1.E — WindBus integration:** simulator event -> binary WindPacket -> decode -> bounded scheduling -> ordered receipt.
- **SIM1.F — native CI:** CMake CTest runs on Linux and Windows and must pass for the exact PR head.
- **SIM2 — later:** protocol-level saved checkpoints (WindStore), model-plug-in behavioral specifications, spatial/temporal multiscale fields, typed biological sensor import, fast-forward and visual replay. Add independent calibration and holdout validation before claiming a digital twin of any physical preparation.

**What is NOT validated:** the neuron transfer function, neuroglial plasticity, immune behavior, RNA trafficking, biological energy dynamics, life-support actions, activity-driven material synthesis, molecular Crystal consumption, biological speech, or human generalization. Those claims require independent experiments and qualification. SIM1 is a *working digital model*, not a proven living computer.
