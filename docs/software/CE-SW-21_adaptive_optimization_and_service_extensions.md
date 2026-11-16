---
title: "CE-SW-21 — Adaptive Optimization and Service Extensions"
project: "ChristEye Anaiero"
version: "0.015.x"
status: "Native synthetic digital-only reference; full simulator forks and durable commits pending"
---

# Adaptive Optimization: distinct from ColonyState

**ColonyStateProjector** is an authoritative materialized **read model** of qualified recent messages. It must not select actions or manipulate simulation entities.

**AdaptiveOptimizationService** reads a coherent ColonyState snapshot, compares a digital metric to an explicitly declared ideal target, asks an injected trial evaluator to test up to 16 candidate digital routes, selects a winner with a bounded goal policy, asks a change port to commit against an exact projector revision, then evaluates later observations for regression/rollback. It cannot mutate ColonyState directly.

~~~text
SIM3/SW5 -> typed WindBus -> weighted priority queue -> ColonyStateProjector
                                                    |          |
                                         postcommit wake      snapshot
                                                    \         /
                                              optimization service
                                              goal -> dry-run trial
                                              -> policy score -> proposal
                                              -> revision-checked digital commit
                                              -> later feedback -> retain/revert
~~~

## Service boundaries and extension points

- **Models** are passive state; no routing, simulated growth, scoring, or persistence logic.
- **Services** own digital simulation, scoring, validation, review and policy. New service interfaces include `ICandidateSource`, `ITrialEvaluator`, `IScorePolicy` and `IRouteChangePort`, plus `IServiceExtension` lifecycle notifications.
- **Infrastructure** owns the route storage backend, packet movement, controller memory and persistence. `InMemoryDigitalRouteStore` is only a RAM demo; future production commit must couple digital route state with an atomic durable outbox.
- **Composition root** injects trusted local strategy implementations at startup. No remote-supplied dynamic code or plug-ins.
- **Existing services**: `RuntimeService`, `ColonyProjector` and `CellBehaviorService` now offer optional read-only lifecycle hooks. `ServiceExtensionDispatcher<4>` fans out without allocation. Hooks are outside projector mutex and must not block or modify state. Older SIM/VLM/learning services require the same design when their logic is extracted into domain modules.

## Score and work scheduling

The default digital score is

`score = 100 * importance * (abs(current-ideal)-abs(predicted-ideal)) - 10*cost - 20*risk - 15*uncertainty`.

A proposal must have strictly positive score and satisfy minimum improvement; deterministic ties use the smaller route ID. The service rejects stale, unqualified or nonsynthetic metrics and a commit based on an outdated snapshot. Subsequent digital feedback can trigger rollback.

The existing 8:5:3:2 weighted queue remains responsible for health observations, digital desires, evidence and telemetry. `OptimizationTriggerService` coalesces post-projection revision notifications without running expensive trials inside the bus callback. The future supervisor must provide a separate bounded budget/task slot for optimization so it cannot delay incoming observations.

## Scientific and safety scope

All this is **digital-only**. A synthetic trial result is a manually supplied proxy delta, not a cell mechanism, calibrated physiology or actual SIM3 branch simulation. The demo route store has no network effects, live-cell effects, physical feeding, biochemical administration, or electrical stimulation. External read-only data cannot authorize adaptive changes. A future integration must implement real digital-state forks, provenance, postcommit outbox, privacy and uncertainty before attempting any claims of fidelity.

## Verification

`anaiero optimize demo` emits a synthetic route-selection result. Tests cover ranking, tie/gain gate, bounded trial count, stale state, nonsynthetic rejection, revision conflict, later feedback/reversal, multi-extension fanout, notifier wake and no projector-lock held during extension calls. Linux and Windows CI run the same tests. The v0.016 transaction and secure mesh remain separate work.
