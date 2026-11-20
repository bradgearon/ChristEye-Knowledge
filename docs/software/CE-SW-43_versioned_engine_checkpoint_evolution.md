# CE-SW-43 — Versioned engine checkpoint evolution (initial implementation)

**Status:** Native SDK review/activate code plus an executable integration fixture on branch feature/anaiero-versioned-engine-evolution. This branch is stacked on PR #53. It is **not yet** a complete state-bearing binary hot-upgrade.

## What is implemented

A compiled native engine may advertise an optional IMigrationCapability. The session owner reviews a **real checkpoint schema conversion** and activates it through the existing canonical commit authority. An indexed sdk.engine-migration record protects the old and new checkpoint blob digests.

The initial contract deliberately holds the engine/module identity, behavior identity, source epoch, configuration, tick, and paused phase unchanged. It does not reinterpret past observations or silently rewrite lifecycle identities.

- MigrationRequest pins operation ID, expected revision, source/target schema, migration implementation SHA-256, and typed parameters.
- IMigrationCapability publishes its implementation digest and prepares a staged transition without authoritative I/O.
- SessionService::review_migration checks the paused engine, exact revision, registered target schema, implementation identity, and unchanged visible state.
- MigrationPlan binds original and transformed checkpoint digests. The canonical binary plan codec refuses malformed or non-canonical plans.
- SessionService::apply_migration recomputes and compares the reviewed result, then commits a new engine state plus indexed migration receipt in one guarded transaction.
- Exact retry resolves the accepted operation and verifies its retained migration plan. Recovery reconstructs from the new checkpoint.

The implementation digest is an exact *byte identity claim*, not an independently authenticated publisher signature or scientific-validity proof.

## Reproduce the functional integration fixture

From repository root, with native Windows/Linux C++20 toolchain:

    cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON -DANAIERO_ENABLE_MESH=OFF
    cmake --build build/native --config Release --target anaiero_engine_migration_tests
    ctest --test-dir build/native -C Release -R '^anaiero_engine_migration$' --output-on-failure

The fixture creates a synthetic SDK counter, trains and pauses it, reviews a counter.checkpoint/1 to counter.checkpoint/2 transformation, rejects stale/tampered requests, commits the update, replays the exact operation ID, resumes learning, and recovers the updated state from canonical storage. It exercises an interrupted pre-ROOT migration and exact retry. This is functional verification, not endurance qualification.

## Follow-on CE-SW-43 increments

1. Add a declared migrator to a production engine, beyond the synthetic SDK fixture.
2. Add native operator review/activate commands and host ownership/admission for paused sessions.
3. Add a deployment-level semantic migration capable of changing module/build identity, with complete scheduled/lifecycle identity updates and explicit compatibility checks.
4. Implement declared reverse transformations or protected pre-migration restoration. An incompatible rollback must refuse; it cannot discard post-upgrade accepted work.

## Restrictions

Migration is only permitted while paused and with no pending canonical operation. A nondeterministic transformer cannot activate its reviewed result. Precommit exceptions and failed guards do not advance the Projector. No claim is made about biological fidelity, transferable learning, physical intervention, or arbitrary binary upgrades.
