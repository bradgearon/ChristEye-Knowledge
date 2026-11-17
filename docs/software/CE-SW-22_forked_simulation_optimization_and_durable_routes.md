---
title: "CE-SW-22 — Forked Simulation Optimization and Durable Digital Route Decisions"
project: "ChristEye Anaiero"
version: "0.015.x / v0.016 preparation"
status: "Native SW5 synthetic-only implementation; authenticated delivery and platform supervisor pending"
classification: "Deterministic digital simulation, never biological intervention"
related: "CE-SW-18, CE-SW-19, CE-SW-20, CE-SW-21, ADR-001, ADR-002"
---

# Anaiero: real digital forks, multi-goal decisions and recoverable route changes

## 1. Purpose and scope

This increment replaces the manually supplied deltas of `SyntheticTrialService` with **actual isolated SW5 `CellBehaviorService` runs**. Given a frozen digital `World` and SHA-256-pinned `CEVCELLATLAS/1`, every candidate is evaluated from the **same baseline World**, under the same digital `Context` and number of simulated ticks. The default evaluator tests `digital_signal` route guidance between two synthetic neurons. Other route kinds are rejected until a corresponding, separately tested SW5 adapter is available.

The `ColonyStateProjector` remains read-only and must not modify cell data. `Sw5ForkOptimizationService` owns candidate evaluation and multi-goal scoring; `DigitalRoutePolicyService` owns digital route transitions; `DurableDigitalRouteService` coordinates state/outbox transactions through `IAtomicRouteLedger`; the filesystem and OS locks live only in `DurableRouteLedger`. Models are passive, and all logic remains in services. The public command is `anaiero optimize fork demo` through the existing `ce_optimization` backend.

All numeric states and "development", "learning" and "support" values are **uncalibrated synthetic proxies**, not evidence that a living cell can be grown, programmed, trained or routed. No physical feeding, stimulation, genotype intervention or material delivery exists in the interfaces.

## 2. Data flow

```text
SW5 World checkpoint + checked synthetic atlas
       |
       v
Sw5ForkOptimizationService
   |-- baseline SW5 clone, run H identical steps
   |-- candidate A clone + digital guidance, run same steps
   |-- candidate B clone + digital guidance, run same steps
   +-- candidate C clone + digital guidance, run same steps
       |
       v
Multiobjective score + hard no-regression guardrails
       |
       v
Selection includes input World SHA, source revision and selected guidance
       |
       v
DurableDigitalRouteService -- IAtomicRouteLedger
       |
       +-- immutable transaction blob:
       |    original World + current World + route + pending receipt
       +-- atomic HEAD publication (the logical commit point)
       |
       v
Outbox receipt (pending until an acknowledged downstream publisher)
       |
       +-- future Win32/Linux authenticated service + WindBus
       |
       v
Monitor later SW5 state -> retain or reverse policy; NO time rewind
```

This is a *single-session* digital route policy transaction. It is not a general distributed transaction across SIM3, SW5, WindStore, or a remote peer. No external `.cevcells` file is modified by the demonstration; the session holds its own original and current sealed SW5 worlds. It allows one active digital route at a time. The pending outbox slot prevents committing another change until the current receipt has been explicitly delivered and acknowledged.

## 3. Multi-objective scoring

`ForkPlan` contains at most six named measures from `viable`, `directed_links`, `total_energy`, `total_activity`, `total_learning`, and `total_process`, their ideal values, weights and hard no-regression flags. Horizon is bounded to 128 ticks and the candidate set to eight digital routes. Each candidate run is isolated, deterministic and side-effect free.

For objective `j`, define `d(x,g)=abs(x-g)`. The score is:

```
S = 100 * Σ_j w_j [d(baseline_forecast_j, ideal_j) - d(candidate_j, ideal_j)]
    - 10*cost - 20*risk - 15*uncertainty
```

A hard-guardrail objective rejects a candidate if its deviation worsens. A candidate also needs a positive score above the plan's declared minimum. Equal-scoring candidates are selected deterministically using their numeric route IDs. The weights, cost, risk and uncertainty parameters are **software trial preferences**, not calibrated physiological effect sizes. There is no optimization against unqualified real-cell data.

## 4. Durable route session and crash behavior

`DurableRouteLedger` holds a single-writer OS lock and commits two stages: (1) create or verify an immutable content-addressed transaction object, and (2) atomically publish a sealed `HEAD` pointing to that SHA-256 object. Every committed transaction contains the complete original and active SW5 worlds, selected digital route, original route weight/presence, revision, parent hash and pending outbox event ID. A crash after storing an object but before publishing HEAD leaves an **unreferenced object**, not an ambiguously active route. Previous HEAD remains valid.

When restarting, the store verifies HEAD, its selected transaction digest, the SW5 world seals, atlas identity, and immediate transaction ancestry/revision. A failed downstream publish leaves the event pending. A successful publish followed by an acknowledgment-write failure may be replayed: downstream consumers **must deduplicate by event identity**; this is at-least-once delivery, not exactly once. The reference leaves events pending unless a consumer explicitly acknowledges them. A complete versioned multi-consumer/outbox recovery layer is still needed before remote transport.

Rollback applies the previous route policy to the **current** SW5 World and publishes a new reversible-policy event; it does **not rewind cell ticks, accrued simulated development, or lineage**. A later digital evaluation can trigger rollback if a protected objective regresses against its prespecified control forecast. This is a digital modeling test only.

The durable writer synchronizes files for this bounded prototype; persistence must move to a dedicated worker before low-latency producer admission. Recovery properties are filesystem-dependent and require independent Linux and Windows crash/power-loss qualification. The route store currently bounds revisions (<=4096), object bytes, and active route count; it is not a long-lived unbounded evidence system.

## 5. Public command, current capabilities and deferred interfaces

```bash
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --target anaiero ce_fork_optimizer_tests
ctest --test-dir build/native -C Release --output-on-failure -R 'anaiero_.*fork'

anaiero optimize fork demo \
  examples/cells/crest-blood-neural.cevatlas ./digital-forks
```

The command creates two **synthetic** neurons without an existing guidance intent, compares three route candidates, writes the chosen route with a pending durable receipt, and outputs the baseline/candidate scores and transaction SHA. Re-running the command on the same session reads the existing HEAD rather than creating a duplicate route. A test-only in-process sink exercises receipt delivery, simulated progression and rollback; it is intentionally not an authenticated remote WindBus, not a live instrument adapter, and not a separately running controller.

What remains: integrate the durable outbox with WindPacket/1 typed route change events (instead of the earlier generic `connection` proxy), register a real local subscriber/acknowledgment path, introduce durable source epochs and per-culture sequence cursors, implement Windows named-pipe/Linux Unix-socket controller services, and run independent real-model calibration before predicting physical outcomes. No physical care or stimulation interfaces are planned in this path.

## 6. Acceptance and instrumentation

- Same SW5 starting World/profile/context/horizon for each fork; baseline unmodified after trials.
- Different candidate digital guidance produces independently simulated candidate connections.
- At least three independent metrics in the multi-goal test, with hard viability guardrails.
- Crash injection after immutable object write does not advance HEAD; restart reads old committed state.
- Reopen and replay pending event with stable identity; explicit acknowledgment yields another transaction.
- Reject unauthorized/stale revision and hash mismatches before commit.
- Advance digital simulation and revert route policy without resetting tick history.
- Cross-platform native C++20 compilation under Windows/MSVC and Linux/GCC, followed by sanitizer and fault-injection tests.
- Operator-approved roots; authenticated mesh and physical sensors remain absent. No biological validation claims.
