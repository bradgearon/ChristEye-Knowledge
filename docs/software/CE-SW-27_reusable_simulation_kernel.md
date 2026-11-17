---
title: "CE-SW-27 — Reusable Simulation Engine Kernel"
product_version: "Anaiero v0.015.x"
status: "Native C++20 SW5/SIM3 adapters and bounded coordinator"
classification: "Digital-only simulation research"
---

# CE-SW-27: Engine lifecycle and simulation composition

The simulation host, adaptive optimizer, scientific coupler and viewer should not
import concrete cell-state structures from one another. The new ISimulationEngine
defines describe, initialize, advance, latest, checkpoint and restore. It is a
lifecycle boundary, NOT a universal biological cell state or an assertion that
two very different numerical models share semantics.

## Implemented

- contracts/simulation_engine.hpp: engine identity, model digest, source epoch,
  bounded digital step and output, injected checkpoint and result sink ports
- application/sw5_simulation_adapter.hpp: the existing CellBehaviorService,
  deterministic SW5 tick, compatible sealed CEVCELLSTATE/1 and actual restore
- application/sim3_simulation_adapter.hpp: the existing durable SIM3 Runner,
  explicit input incompatibility, state digest and read-only checkpoint export.
  Arbitrary rewind is unsupported; SIM3 must be reopened through its committed
  culture.state and writer lock.
- services/simulation_coordinator_service.hpp: at most eight engine slots,
  explicitly selected cadence per engine, duplicate identity rejection and
  bounded post-tick publishing retry.
- infrastructure/memory_checkpoint.hpp: bounded test checkpoint port.

## Run

    anaiero kernel demo examples/cells/crest-blood-neural.cevatlas \
      examples/connected-gardens.cev ./kernel-session

In this reference demonstration, SW5 steps every controller cycle and SIM3
steps every second. The units remain synthetic proxies. SW5's internal World
is never cast into a SIM3 Cell or a NEURON compartment. Models remain passive;
services implement behavior; application adapters translate the lifecycle.

## Explicit nonclaims

SimulationCoordinatorService does NOT yet implement atomic transactions over
multiple engine states. Each engine's own checkpoint/commit rules continue
to apply. Failed snapshot publication blocks further steps until the pending
snapshot is retried, but the service is not itself a global durable outbox.
A future group coordinator must handle a multi-engine commit and recovery
journal, rather than treating independently committed ticks as one transaction.

SW5 and SIM3 use distinct models; equal tick numbers or metrics are not proof
of equivalent physics. No biology is controlled, and synthetic success is
not independent experimental validation.

## Release gates

Native MSVC and GCC builds, identical SW5 deterministic checkpoints in two
engine instances, SW5 restore/replay, SIM3 persisted reopen, duplicate source
identity rejection, and bounded publisher failure/retry tests.
