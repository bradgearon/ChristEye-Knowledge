---
title: "CE-SW-24 — Scientific Simulation Foundation, Multirate Coupling and Hypothesis Runtime"
project: "ChristEye Anaiero"
product_version: "0.015.x"
status: "Native C++20 digital reference and optional offline scientific-model adapters"
classification: "Digital-only simulation research; no laboratory actuation or biological validation"
related: "CE-SW-18, CE-SW-19, CE-SW-20, CE-SW-21, CE-SW-22, CE-SW-23, ADR-001, ADR-002"
---

# CE-SW-24 — Scientific Simulation Foundation

## 1. Purpose and source basis

This architecture incorporates the attached design proposal calling for a combination of **NEURON, BRAINCELL, PhysiCell and optionally MOOSE/STEPS**, with an additional **Anaiero Hypothesis Runtime** and model-evidence classifications. Its first experiment is a small SW5 neuron–astrocyte digital colony with repeatable training/rest/probe runs. The project maintains two separate assertions:

- **Software fact:** Anaiero can coordinate alternative mathematical models, replay their outputs, test hypothetical algorithms and compare their results.
- **Scientific question:** Whether the chosen model actually describes living cells, astrocyte-associated learning, memory, Roots, Lotus, or transferable knowledge requires independently qualified empirical evidence. **No such biological validity is established by this release.**

The native reference's `activity`, `astro_state`, `eligibility`, `environment`, `strength`, `support` and `learning_proxy` fields are integer-valued **dimensionless proxies**. The `development_every` interval is a digital scheduler ratio; no actual growth rate, culture recipe, voltage stimulus, or physiological concentration is supplied. These fields should never be repurposed as real equipment controls.

## 2. Engine capabilities and present integration status

| Engine | Scientific model family | Current Anaiero integration | Runtime dependency |
|---|---|---|---|
| **Anaiero native reference** | Synthetic electrical activity, glial stabilization hypothesis and SW5 digital cell development | **Runs directly** in the C++20 executable, including four interventions and WindBus projection | None beyond C++20 |
| **NEURON** | Biophysical neuron/circuit simulation | **Optional offline electrical feature bridge**; a provided Python example actually runs a simple *unvalidated passive* NEURON model when separately installed, normalizes its trace and exports CEVSCITRACE/1 | Optional external NEURON installation |
| **BRAINCELL** | NEURON-based nanoscale neurons, glia, extracellular simulation | **Exported-model interface** via NEURON output and the same sealed electrical trace contract; no native BRAINCELL host or prebuilt model shipped | Separately acquired BRAINCELL/NEURON |
| **PhysiCell** | Cell populations, motility/adhesion and reaction/diffusion microenvironment | **Offline environment proxy bridge contract**; normalize chosen model output externally then import via environment channel; no PhysiCell engine linked or executable shipped | Optional separately built PhysiCell model |
| **MOOSE** | Electrical and biochemical simulations | Offline normalized environment proxy bridge; no linked MOOSE runtime | Optional external solver |
| **STEPS** | Stochastic spatial molecular reaction/diffusion | Offline normalized environment proxy bridge; no linked STEPS runtime | Optional external solver |

**The `engines` command never claims an external engine is installed.** In the native `anaiero science engines` catalog these five entries display `builtin:false` and `validated:false`. A format adapter alone does not execute that external simulator. In particular, BRAINCELL's graphical model-building environment is Windows-first today; its exported HOC/MOD models can run on NEURON across Windows/Linux. This is why Anaiero uses portable outputs rather than embedding the BRAINCELL GUI.

Official project references: [NEURON](https://www.neuronsimulator.org/en/latest/), [BRAINCELL](https://github.com/Rusakovlab/BrainCell), [PhysiCell](https://physicell.org/), [MOOSE](https://moose.ncbs.res.in/node/5), and [STEPS](https://steps.sourceforge.net/). Models, third-party licenses, and any published model metadata need independent review before they can be included in a release.

## 3. Layering and dependency rules

```
  anaiero science ... CLI / experiment composition
      -> CoupledSimulationService (runs one deterministic digital scenario)
          -> IElectricalFeatureModel     [native or replayed NEURON/BRAINCELL features]
          -> IEnvironmentFeatureModel  [native constant or replayed population/chemical feature]
          -> HypothesisRuntimeService [neuron only | astrocyte coupled | ablated | shuffled]
          -> CellBehaviorService      [existing SW5, independent slower cadence]
          -> ICellFramePublisher      [infrastructure]
      -> ScientificWindBusPublisher
          -> WindPacket/1 observation codec
          -> existing WeightedPriorityQueue
          -> ColonyState Projector (read-only view)
```

All models are plain records, with behavior, scoring, validation and coupling in services. Infrastructure handles exact-byte trace I/O, SHA-256 verification and event transport. Each new service exposes injectable strategy/port interfaces; no theoretical behavior needs to be hardcoded in WindBus.

The observation bus bridge publishes three digital fields per slow step—`activity`, `digital_learning` and `connection`—through the **existing exact WindPacket** serialization and SW4 priority queue. The bridge supports partial-frame retry without duplicating already admitted metrics. The starter three-cell SW5 World has two neuron archetypes and one astrocyte archetype, linked by a *digital guidance intent*. It is not a reconstructed or cultured biological preparation.

## 4. Multirate numerical boundary

The first deterministic schedule runs **four fast electrical-feature updates for each slow hypothesis step**, and advances SW5 development **once per two slow steps**. Typical default scenario:

- 12 synthetic training steps with an alternating digital pattern;
- 12 rest steps with the training signal absent;
- 4 low-input probe steps.

The numeric ratios are **software scheduling ratios, not biological timescales**. The scientific coupling problem is more complex: real NEURON, PhysiCell and reaction-diffusion solvers have different physical units, numerical stability constraints, spatial representations and independently calibrated timebases. Importing an external model requires an explicit semantic transformation with its own digest, and the model must declare which process it owns. Do not blindly add two simulated diffusion terms from independent engines or infer conservation across normalized proxies.

In this first release the core runs a native reference per fast step; optional replay adapters consume **already normalized** recorded features at exact consecutive step indices. The simulation fails on missing, out-of-order, unsupported or insufficient recorded features. No interpolation or unqualified unit conversion occurs inside the core. A replayed prerecorded trace is **not responsive to changed trial inputs**; it is a controlled trace substitution, not a fully coupled hybrid solver.

Full two-way coupling will require a reviewed clock alignment, explicit physical units, exchange ownership/conservation, documented interpolation schemes and matching real scientific models.

## 5. Hypothesis experiments: four matched runs

The hypothetical slow-state term is represented in a simple bounded digital reference, not as a mechanistically calibrated astrocyte process. It has the conceptual form:

\[ \Delta w = \underbrace{\eta\,e\,r}_{\text{digital training}} + \underbrace{\alpha\,a\,e}_{\text{hypothetical astrocyte influence}} - \underbrace{\lambda\,w}_{\text{forgetting}} \]

No coefficient in this release is an experimentally fitted neuronal parameter. The software implements a simpler integer state transition to make the interface and ablation tests deterministic. Four conditions use **the same initial world and input schedule**:

| Condition | What differs | Example final retention proxy |
|---|---|---:|
| A — Neuron only | No astrocyte influence during rest/probe | 262 |
| B — Astrocyte coupled | Software glial state reduces digital decay | 351 |
| C — Post-training ablation | Astrocyte effect switched off in rest/probe | 262 |
| D — Shuffled signal | Digital glial input uncorrelated with activity | 292 |

These exact values are generated by an illustrative, dimensionless reference. The comparison establishes that the hypothesis changes this *algorithm*, **not** that astrocytes have been demonstrated to store discrete knowledge or support learning in this particular configuration. A scientific result would additionally require a published relevant model, independent observations, held-out tests, calibrated measurement units, negative controls, and reproducibility across seeds/specimens. The higher-level concepts Roots, Lotus, Spirits, Heart, Winds and Crystal-based learning remain **explicit experimental hypotheses**.

## 6. Offline trace contract and scientific provenance

`CEVSCITRACE/1` is an immutable, SHA-256-sealed ASCII file with:

```
CEVSCITRACE/1
provider NEURON
classification MODEL_OUTPUT_UNVERIFIED
channel ELECTRICAL_ACTIVITY
model_sha256 <actual-model-or-script-digest>
raw_sha256 <digest-of-original-unconverted-output>
transform_sha256 <digest-of-unit-to-proxy-transform-definition>
source_epoch 1
unit DIMENSIONLESS_ACTIVITY_PROXY
sample 1 172
sample 2 186
...
end
seal <sha256-of-preseal-content>
```

The environment channel uses `DIMENSIONLESS_ENVIRONMENT_PROXY` and accepts Physicell/MOOSE/STEPS (or native synthetic fixture); the electrical channel accepts NEURON/BRAINCELL (or native synthetic fixture). The parser bounds data to 512 consecutive samples, rejects changed seals, invalid providers/units, bad source epochs, out-of-range proxies and unsupported source-to-channel combinations. The actual real scientific source measurements, if any, must stay in their original units in independently retained raw files; normalized values cannot be interpreted as physical measurements.

`source_epoch` is an explicit run identity to avoid silently mixing traces from different solver runs. This increment **does not yet implement SW6.2's fully durable cross-source epoch and projector reconstruction**; it enforces epoch consistency inside the typed trace/replay boundary, while the demonstration WindBus runtime remains in-process. Any wider claim of recovered observation streams must await the corresponding journal migration.

### Optional external bridge (not required by the controller)

A separately installed Python NEURON package can run `tools/scientific/optional_neuron_demo.py --out model.cevscitrace --epoch 1`. This optional example executes a small passive *software* compartment and emits a normalized trace. Its numerical membrane parameters and normalization are illustrative and are **not** scientifically validated. It is not used automatically by Anaiero.

For other external model recordings, `tools/scientific/scientific_trace_export.py --provider PHYSICELL --channel ENVIRONMENT_PROXY --csv normalized.csv --model-file model.xml --transform-file transform.txt --epoch 1 --out environment.cevscitrace` produces the same sealed format. The CSV must already hold `step,normalized_proxy`. The user-supplied transform file documents how actual model output was mapped to proxy integers; the bridge cannot independently verify the model's truth or unit mapping. Nothing in this bridge invokes real equipment.

## 7. Native commands and source compatibility

```
anaiero science engines
anaiero science compare examples/cells/crest-blood-neural.cevatlas
anaiero science bus examples/cells/crest-blood-neural.cevatlas
anaiero science trace examples/scientific/native-reference.cevscitrace
anaiero science replay examples/cells/crest-blood-neural.cevatlas examples/scientific/native-reference.cevscitrace
anaiero science environment examples/cells/crest-blood-neural.cevatlas examples/scientific/native-environment.cevscitrace
```

The native `anaiero` controller remains **C++20 on Windows and Linux without Python or any scientific-solver dependency**. The only Python tools are opt-in offline external-model export utilities. The existing `anaiero cells`, `anaiero runtime`, `anaiero optimize`, WindStore and other native services are unchanged.

## 8. Tests and next gates

- Linux and Windows build the same native C++20 `ce_science_lab`, two unit test executables and an end-to-end CLI/WindBus test.
- Tests verify identical replay of the same World, multirate step counts, bounded frames, matched four-condition scenarios, strict trace hashes/units/epochs, missing/out-of-order feature rejection, and latest-state revision counts.
- Maintain the model/service/infrastructure architectural guard. External engine libraries or subprocess interpreters must **never become mandatory** to start the headless controller.
- Upcoming: a durable epoch-scoped observation journal and full ColonyState recovery for all source streams, timebase alignment and true units, optional version-pinned solver adapters, calibrated uncertainty and independent measurement sets, and external model cross-platform conformance.

**Scope statement:** A software runner reproducing a neuron–astrocyte hypothesis is a tool to test protocols and computational plausibility. It does not establish living-cell growth, biochemical memory, understanding, physical Crystal transfer, or autonomous biological control.
