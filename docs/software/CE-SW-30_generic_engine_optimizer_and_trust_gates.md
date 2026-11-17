---
title: "CE-SW-30 — Engine-Neutral Optimization and Verified Session Policy"
product_version: "Anaiero v0.015.x"
status: "Native C++20 engine-neutral digital optimization reference"
classification: "Digital-only; authenticated remote mesh is not implemented"
---

# CE-SW-30 — Reusable scenario ranking, SW5 forks and trust boundaries

## Why an independent scenario evaluator

The previous SW5 optimizer contained both the candidate-branch simulation
algorithm and the scoring decision. This module separates those concerns.
EngineOptimizationService now accepts an IScenarioEvaluator and evaluates
bounded alternative policy tokens without knowing SW5 cell structures, SIM3
entity layouts or numerical solver internals.

Two forecasts must start from the same pinned simulation source, use the same
source epoch and run to the SAME horizon. The optimizer never compares a
later trial forecast with an earlier untouched baseline as if that were
the treatment effect. Each candidate is evaluated from the same initial World.

## Implemented

- IScenarioEvaluator, IEngineChangePort and pure passive Candidate, Forecast,
  Selection and Objective records in contracts/engine_optimization.hpp.
- EngineOptimizationService: up to four distinct goals, eight candidates,
  declared ideal targets, deterministic ties, cost/risk/uncertainty penalties
  and no-regression rejection. The service does not know any specific engine.
- Sw5ScenarioEvaluatorService: actual, isolated SW5 model World forks under
  a specified synthetic Context, same horizon, no mutation of source World.
- Sw5EngineChangeAdapter: explicitly maps a chosen generic candidate back to
  the SW5 immutable route option. Commit uses the preexisting
  DurableDigitalRouteService which checks the original snapshot digest,
  source revision, active route state and pending delivery.
- contracts/verified_session.hpp and SessionAuthorizationService: remote
  session capability policy for subscribe, synthetic observation publish and
  propose-digital-change, requiring source+epoch binding and verified identity.

## Trust limitations

An injected IVerifiedSession must be established by an independently secure
TLS/QUIC or other authenticated transport. These interfaces DO NOT establish
cryptographic verification, issue certificates, enroll a network peer or
deliver remote subscriptions. No valid packet hash or self-reported slot
is sufficient authorization. Real lab devices and culture intervention
are not connected to this contract.

## Engine support and next gates

SW5 is a real digital evaluator and durable digital route-commit adapter.
SIM3 and external NEURON/PhysiCell models can adopt the same scenario and
checkpoint port, but a matched candidate policy and independent replay must
be implemented before calling them active optimization backends.
The synthetic rules are not experimentally validated cell behaviors.

Windows and Linux native tests cover comparing real SW5 branches at equal
forecast horizons, source digest/revision matching, deterministic selection,
actual durable route commit, immutable pending outbox and negative source,
epoch, capability, expiry and unverified-session authorization checks.

These changes preserve existing WindPacket and CEVCELLSTATE formats.
