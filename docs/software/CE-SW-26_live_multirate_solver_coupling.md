---
title: "CE-SW-26 — Optional Live Multirate Scientific Solver Coupling"
project: "ChristEye Anaiero"
product_version: "0.015.x"
status: "C++20 live step/reply reference and optional NEURON sidecar"
related: "CE-SW-19, CE-SW-24, CE-SW-25, ADR-002"
---

# SW7.1 — Live external model stepping through a portable C++20 controller

## Design basis

The supplied simulation proposal calls for combining validated or published scientific numerical engines (NEURON/BRAINCELL, PhysiCell and optionally MOOSE/STEPS) with **explicit Anaiero research hypotheses**, while distinguishing published modeling methods from unverified custom rules. **A framework's established numerical capabilities are not proof a particular cell model is accurate.** NEURON implements electrical/dynamical models; its documented `finitialize` and `fadvance` advance a user-specified numerical experiment. Other engines need independently configured models and explicit coupling adapters.

Anaiero does not embed those heavyweight engines in its minimal Windows/Linux controller. Instead `CoupledSimulationService` from CE-SW-24 continues coordinating SW5 and hypothesis services, and new `LiveElectricalFeatureService` and `LiveEnvironmentFeatureService` adapt a separately stepped numerical model through an injectable port `ILiveSolverExchange`.

## Exact runtime topology

~~~text
optional external numerical model (separately installed NEURON)
          <-> bounded STEP/SAMPLE stdio protocol
              | source epoch, model hash, step sequence,
              | logical elapsed_ns, time uncertainty, normalized feature
              v
LiveElectricalFeatureService (4 micro-steps / macro step)
LiveEnvironmentFeatureService (1 environment feature / macro step)
              v
CoupledSimulationService
  -> replaceable HypothesisRuntimeService
  -> existing SW5 CellBehaviorService (every 2 macro steps)
  -> EpochScientificFramePublisher (4 event metrics per macro step)
      -> EpochObservationDeliveryService
         -> WindStore committed observations
         -> WindPacket v2 -> WeightedPriorityQueue -> ColonyState projector
~~~

The default digital example is 28 macro steps, 112 fast electrical steps and 28 slower environmental steps. The deliberately chosen **simulated** microstep is 25,000 ns (0.025 ms); slow step is 100,000 ns. These values are schedule/protocol parameters, not an instrument stimulation schedule or a biological development rate. SW5 updates every second macro step, giving a clearly distinct third, slower cadence.

### Example framing

The native process writes:
`STEP E 1 1 25000 320 <model_sha256>`

The external numerical-model process replies:
`SAMPLE E 1 1 25000 505 0 <same_model_sha256> OK`

`E` is the normalized digital electrical feature and `M` the digital environment feature. The protocol validates channel, epoch, sequence, elapsed logical time, bounded feature, uncertainty and model digest before accepting a response. It uses **strictly bounded lines** and no native C++ struct casts. Stdio is local IPC, not authenticated network transport. The model hash pins the bridge's declared source content; it does not prove a published model's validity or that a supplied data stream came from a trusted physical source.

### Optional NEURON bridge

`tools/scientific/live_solver_bridge.py --backend neuron` starts the native Anaiero process, runs a small separately installed NEURON numerical section, calls NEURON's `h.fadvance()` for each C++ request, and returns a **predeclared, illustrative mV-to-proxy transformation**. This is an actual bidirectional, stepwise numerical coupling, not a precomputed trace import. It does not validate a learning mechanism or a specific real cell. No physical current, actuator or experiment control is performed.

The same script's dependency-free `--backend synthetic` mode drives the identical protocol for CI. The slower `M` channel is **still synthetic** in both modes; no PhysiCell/MOOSE/STEPS installation or direct live connector is claimed. A future `PhysiCell` sidecar can implement this versioned `M` protocol when a documented, calibrated feature extraction and conservation/clock contract exists. A BRAINCELL model may be supplied to an installed NEURON process, but the current example does not bundle a BRAINCELL model.

## Fail-closed semantics and pending limitations

- The native C++ core never requires Python/NEURON; Python is only an optional external sidecar/test driver. Windows and Linux execute the same native coordinator and protocols.
- Before any live steps, the controller records a separate, immutable `SCIENTIFIC_MODEL_SESSION` object with **the complete declared model SHA-256**, atlas digest, epoch, provider label, transform identifier and numerical timesteps. A given session epoch cannot be reused, even if the external solver fails before emitting an observation. This is a local provenance record, not a cryptographic signature or independently validated NEURON model.
- The external model may advance some microsteps before a macro-step completes. If a response fails, Anaiero **aborts the model session**. There is no justified rollback of an already advanced external solver, and the session may only restart with a new source epoch.
- `StdioLiveSolverExchange` validates framing but does not itself impose an OS-level read timeout or crash monitor; a future host-service adapter must enforce deadlines, process identity, and bounded I/O worker ownership to avoid indefinite blocking.
- The current bridge returns normalized **dimensionless features**, not physical concentration, membrane voltage, ion flux, cellular metabolism or a verified NEURON/PhysiCell coupling. That normalized conversion must be replaced with an explicit unit-aware transformation and source-calibration record before meaningful cross-solver physical interpretation.
- Native WindStore persistence synchronizes to disk on the macro-step delivery path; separating that into an asynchronous, bounded worker is an upcoming production requirement. No hard real-time guarantee is claimed.
- A successful synthetic learning proxy is not biological memory or a demonstration of Anaiero's higher-level Root/Lotus/Spirit/Crystal hypotheses.

## Commands

~~~sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --target anaiero

anaiero science epoch-demo examples/cells/crest-blood-neural.cevatlas ./epoch-store

python tools/scientific/live_solver_bridge.py \
  --anaiero ./build/native/anaiero \
  --atlas examples/cells/crest-blood-neural.cevatlas \
  --store ./scientific-live-store --epoch 1 --backend synthetic

# Only if NEURON is separately installed in that Python environment:
python tools/scientific/live_solver_bridge.py \
  --anaiero ./build/native/anaiero \
  --atlas examples/cells/crest-blood-neural.cevatlas \
  --store ./scientific-live-store --epoch 2 --backend neuron
~~~

For Windows, provide the native path to `anaiero.exe`; no WSL is required. This release does not test an actually installed external NEURON solver in CI, so that optional backend is **not yet qualified**. Official NEURON programmatic-control reference: https://www.neuronsimulator.org/en/latest/progref/simctrl/programmatic.html.

## Acceptance

- Windows/MSVC and Linux/GCC C++20 conformance: exact source epoch, model hash, channel, integer bounds, sequence, elapsed time, fail-closed output.
- Step-count test: 28 macro steps = 112 electrical and 28 environment replies, with 14 SW5 developmental steps.
- Public native CLI orchestration through the dependency-free synthetic sidecar (normal and malformed reply), with source epoch 1 then 2 and durable WindStore/ColonyState reconstruction.
- No unbounded per-message queues, no hidden third-party runtime requirement for `anaiero`.
- Optional NEURON actual backend independently verified only when a compatible NEURON package is installed.

Future: portable process supervisor with deadline/exit handling; true live PhysiCell adapter, model-specific physical units, conservation budgets, and controlled multirate state-checkpoint negotiation.
