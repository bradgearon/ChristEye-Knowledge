# CE-SW-36 — Native environment and frozen calibration evidence

This batch adds a native environmental model and a bounded calibration path to
the supervised scientific sessions in [CE-SW-34](CE-SW-34_supervised_scientific_sessions.md).
It composes with the durable sessions, reusable experiments and evidence mesh;
it does not require an external solver or Python in the controller.

## Integrated capability

| Area | Implemented behavior |
| --- | --- |
| Scientific quantities | One declared scalar per scientific engine, with distinct voltage and concentration schemas, metrics and units |
| Environmental model | Native exact exponential integration of a fixed-volume, well-mixed concentration model |
| Process and session lifecycle | Existing owned child process, deadlines, cancellation, complete checkpoints and durable publication |
| Reference provenance | Sealed reference series with source, epoch, clock, run, protocol, producer and pinned raw artifact identities |
| Calibration | Finite parameter candidates selected using training data, followed by one evaluation of the frozen winner on a separate holdout |
| Operating surface | Native configuration and calibration commands, JSON evidence reports, mixed sessions and Windows/Linux CI |

**Design Decision:** environmental concentration is a generic modeled scalar.
No chemical species, biological culture, sensor calibration, or voltage-to-
concentration coupling is implied by a micromolar unit. Provider, model and
behavior digests identify the declared computation. Actual species-specific
models and independently validated measurements require additional evidence.

## Ownership and extension points

`EnvironmentCompartmentModel`, `EnvironmentCompartmentState` and
`EnvironmentBalance` are passive model records. `EnvironmentCompartmentService`
owns numerical validation and state transitions. Its file adapter parses bounded
model bytes and computes their digest. The provider executable handles the
existing scalar solver protocol and delegates all numerical decisions to the
service.

`ScientificObservable` is a shared declaration of supported transport semantics.
The configuration reader, solver exchange, engine service, native session and
JSON view use that declaration. Existing numeric IDs remain unchanged;
concentration appends a new summary and observation metric. The passive voltage
schemas remain `0xCE710001` / `0xCE710002`; concentration uses
`0xCE720001` / `0xCE720002`. Existing `CEVSOLVER/1`, `CEVSOLVERSTATE/1`,
`CEVSCIENCEENGINE/1` and session formats retain their original interpretation.
An older binary that does not know the appended concentration metric rejects it.

Calibration separates `IEnvironmentTrial`, `IFidelityPolicy`, `ICancellation`
and `IObserver` from file loading and report publication. The default trial
uses the native environmental service. Selection and fidelity decisions belong
in `EnvironmentCalibrationService`; filesystem mechanisms belong in the file
adapter. Compiled observers must not block or reenter their owner.

The Projector remains observational. Scientific session engines still declare
candidate evaluation and live application unsupported. The offline calibration
command produces evidence and a selected model identity; it does not mutate an
active session or enable automatic scientific optimization.

## Environmental equation and numerical contract

**Design Decision:** for one fixed-volume compartment and input held constant
through a step, define concentration $C$ in micromolar and simulated time $t$
in seconds:

$$
\frac{dC}{dt}=a(C_{in}-C)-kC,\qquad
a=\frac{10^9}{\mathrm{exchange\_tau\_ns}},\quad
k=\frac{\mathrm{consumption\_ppm\_per\_second}}{10^6}.
$$

The configured inlet fraction is an integer from 0 to 1,000:

$$
C_{in}=\mathrm{inlet\_max\_micromolar}\frac{u}{1000},\qquad
C_* = \frac{a C_{in}}{a+k},\qquad
C(t+\Delta t)=C(t)e^{-(a+k)\Delta t}
             +C_*\left(1-e^{-(a+k)\Delta t}\right).
$$

The implementation uses monotone `lerp`, `expm1` and a short-series form of the
integrated exponential near zero to avoid drift at equilibrium and cancellation
in very short steps. It preserves
the full double-precision concentration in checkpoints. The scalar sample is
rounded to integer micromolar and carries a one-micromolar numerical envelope.
That envelope describes numerical sampling; it is not measurement uncertainty.

The service also exposes the latest step's fixed-volume concentration budget:

$$
C_{before} + aC_{in}\Delta t
 -a\int C\,dt-k\int C\,dt-C_{after}=r.
$$

`EnvironmentBalance` reports before, supply, exchange-out, consumption, after
and residual $r$, all in micromolar. This is a concentration budget for a
constant volume, not an amount-of-substance measurement. Very large opposed
fluxes lose precision when subtracted; the diagnostic exposes the residual,
and numerical tests use a scale-dependent floating-point tolerance. It does not
promise exactly zero residual for every allowed stiff parameter combination.
Restore supplies a neutral identity budget until the next local step, rather
than reconstructing unrecorded historical fluxes.

| Parameter or operation | Accepted bound |
| --- | --- |
| Initial and maximum inlet concentration | 0–1,000,000 micromolar |
| Exchange time constant | 1–1,000,000,000,000 ns |
| Consumption coefficient | 0–1,000,000,000 millionths per second |
| Simulated step | 1–1,000,000,000 ns |
| Inlet input | Integer 0–1,000 |
| Provider sequence | At most 1,000,000 steps |
| Model file | At most 4 KiB |
| Solver request/response | Existing 16 KiB bound |

Initialization, stepping and restore reject invalid parameters, out-of-order
sequence, nonfinite state and concentration outside the model's bounds.
Checkpoint compatibility binds the model, behavior, provider, source epoch,
step, unit, configured input and full numerical state. Changing the executable
or model also changes the configured session identity.

## Scientific session integration

An environmental provider reports `native-environment-analytic`, `Unit::micromolar`
and checkpoint schema 1 during the existing identity handshake. A mismatched
unit, behavior, model or epoch fails before the runtime publishes a value.

The scientific engine's compatibility snapshot uses the existing scalar slot,
but its capability descriptor now specifies `concentration_micromolar`.
`NativeSessionService` resolves that semantic to `environmental_concentration`;
it does not reinterpret concentration as energy or membrane voltage. Session
views include the model digest, behavior, classification, source, epoch, clock
domain, simulated time step and unit.

Publication retains the established sequence: durable engine checkpoint,
publication intent, committed observation, projection and saved continuation.
A mixed session can run SW5, SIM3, passive voltage and environmental concentration
at distinct cadences. Identical tick counts do not imply synchronized physical
clocks. This batch does not add coupling between those independent models.
Offline mesh export and the existing observation codec preserve the appended
metric; peers require a binary that understands it.

## Calibration and reference evidence

`CEVENVREFERENCE/1` identifies a bounded trace with explicit concentration units,
initial concentration and uncertainty, step and contiguous sample times.
Each sample declares its inlet input, observed/reference concentration and
uncertainty. Provenance carries source, epoch, clock, run ID, producer or
acquisition digest, protocol digest and raw artifact digest. The loader checks
the pinned raw bytes as well as the sealed series; those hashes establish byte
identity, not an authenticated instrument or a verified extraction procedure.

The origin must be `SYNTHETIC_REFERENCE` or `EXTERNAL_UNVERIFIED_READONLY`.
Synthetic examples qualify the numerical implementation. Imported data remain
explicitly external and unverified even when their error criteria pass.
Recorded reference uncertainty is reported separately from raw prediction
error and is never silently subtracted from error or used to relax a threshold.

`CEVENVCALIBRATION/1` pins the training series, holdout series, finite model
candidates and maximum absolute/RMSE errors before evaluation. Both independent
runs declare the same initial condition and protocol; candidates preserve that
initial condition. The command supports at most 32 candidates and 4,096 samples
per trace. Every trial starts afresh, with only the trace's declared inlet
schedule as input.

1. Validate the plan, model and artifact pins, units, source identities, initial
   conditions, bounds and sample chronology.
2. Reject direct split reuse: identical reference or raw artifact digests,
   identical run IDs, or the same source/epoch pair.
3. Evaluate every candidate against training data only and minimize integer
   squared error. Ties retain the original plan order.
4. Freeze the selected candidate and its model digest.
5. Evaluate that one candidate against the holdout. A failure does not trigger
   a search for a candidate that performs better on the holdout.
6. Publish a report with separate training and holdout decisions. Overall
   acceptance requires both to satisfy the predeclared criteria.

The service uses bounded integer error accumulation; RMSE is rendered for
readability while acceptance compares squared quantities. Source/run labels and
hash checks detect direct reuse. They cannot establish experimental independence
or prevent an operator from presenting relabeled or transformed copies as new
measurements. A scientific review must establish that provenance separately.

The report records the selected model, behavior, plan and reference hashes,
declared provenance, candidate training errors, held-out errors, criteria and
decisions. A passing report does not grant live execution authority. Existing
reports are not overwritten; publication exposes only a complete new report.

## Running the capability

Build the native tools, including a controller-only profile without Python:

```sh
cmake -S native -B build/controller -DANAIERO_CONTROLLER_ONLY=ON \
  -DCE_WIND_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
cmake --build build/controller --config Release --parallel 2
```

On Linux, create a pinned environmental configuration and run/restart it:

```sh
build/controller/ce_scientific_config environment \
  build/controller/ce_environment_solver_provider \
  examples/scientific/well-mixed-environment.cevmodel \
  run/science/environment.cevsolver
build/controller/ce_colony_runtime session \
  run/science/environment.cevsession run/science/state 40
build/controller/ce_colony_runtime session \
  run/science/environment.cevsession run/science/state
build/controller/ce_scientific_calibrate \
  examples/scientific/calibration/reference.cevcalibration \
  run/science/qualification.json
```

For a native Visual Studio build, use the corresponding PowerShell commands:

```powershell
.\build\controller\Release\ce_scientific_config.exe environment `
  .\build\controller\Release\ce_environment_solver_provider.exe `
  .\examples\scientific\well-mixed-environment.cevmodel `
  .\run\science\environment.cevsolver
.\build\controller\Release\ce_colony_runtime.exe session `
  .\run\science\environment.cevsession .\run\science\state 40
.\build\controller\Release\ce_colony_runtime.exe session `
  .\run\science\environment.cevsession .\run\science\state
.\build\controller\Release\ce_scientific_calibrate.exe `
  .\examples\scientific\calibration\reference.cevcalibration `
  .\run\science\qualification.json
```

The complete build also exposes `anaiero calibrate <plan> <new-report>`.
The configuration utility uses engine 302/source 82 for the environmental
example, distinct from passive voltage's engine 301/source 81. To compose the
generated configurations, place the following session beside both solver files:

```text
CEVSESSION/2
name voltage-and-environment
period_ms 100
storage segmented 256
engine 301 scientific 81 1 1 "passive.cevsolver" "-" 0 0 0 0
engine 302 scientific 82 1 2 "environment.cevsolver" "-" 0 0 0 0
end
```

Generate `passive.cevsolver` using the existing `ce_scientific_config native`
command from the [scientific examples guide](../../examples/scientific/README.md).
The supplied environmental setup uses a 100 ms simulated step; the existing
passive setup uses 25 microseconds. The session's wall-time scheduling period is
separate from those model clocks.

## Verification and limits

The environment suite compares the exact update against an independent RK4
integration, exercises influx/decay/equilibrium and stiff/short-step bounds,
checks the scale-dependent flux residual, and restores full state into a fresh
process for exact continued checkpoints and snapshot hashes.

The session and CLI suites cover both scientific quantities beside SW5/SIM3,
distinct cadences and clock declarations, recoverable publication, journal and
Projector state, persisted semantic contracts, offline export, correct views,
configuration tampering and legacy voltage behavior. Calibration tests cover
frozen selection, failing held-out evidence, invalid provenance/splits/units,
model and artifact tampering, error bounds, extension behavior and complete
report publication. Native Windows and Linux are separate required CI jobs;
cross-compilation alone is not treated as Windows validation.

These are numerical and software acceptance tests. They do not demonstrate
biological calibration, neural learning, reaction-diffusion, spatial transport,
live acquisition, environmental control or multi-model scientific optimization.
The next scientific increment needs domain-specific model semantics and actual
independently qualified reference data before expanding those claims.
