# CE-SW-43 — Versioned engine checkpoint evolution (initial implementation)

**Status:** Native SDK review/activate code, an actual Cell Behavior v1→v2 migrator, review/activate/status CLI commands, and executable integration fixtures on branch feature/anaiero-versioned-engine-evolution. This branch is stacked on PR #53. It is **not yet** a complete state-bearing binary hot-upgrade.

## What is implemented

A compiled native engine may advertise an optional IMigrationCapability. The session owner reviews a **real checkpoint schema conversion** and activates it through the existing canonical commit authority. An indexed sdk.engine-migration record protects the old and new checkpoint blob digests.

Cell Behavior now accepts a legacy CEVCELLSTATE/1 payload or a new CEVCELLSTATE/2 payload that wraps the same bounded world bytes with a SHA-256 integrity check. New steps preserve whichever format was committed. No biological behavior is altered by the wrapper. The initial contract deliberately holds the engine/module identity, behavior identity, source epoch, configuration, tick, and paused phase unchanged. It does not reinterpret past observations or silently rewrite lifecycle identities.

- MigrationRequest pins operation ID, expected revision, source/target schema, migration implementation SHA-256, and typed parameters.
- IMigrationCapability publishes its implementation digest and prepares a staged transition without authoritative I/O.
- SessionService::review_migration checks the paused engine, exact revision, registered target schema, implementation identity, and unchanged visible state.
- MigrationPlan binds original and transformed checkpoint digests. The canonical binary plan codec refuses malformed or non-canonical plans.
- SessionService::apply_migration recomputes and compares the reviewed result, then commits a new engine state plus indexed migration receipt in one guarded transaction.
- Exact retry resolves the accepted operation and verifies its retained migration plan. Recovery reconstructs from the new checkpoint.

The migration digest is a module-declared algorithm fingerprint, not proof of the compiled bytes or publisher. The separate exact module/build identity pins executable compatibility; neither proves scientific validity.

## Native session operator workflow

Use the existing canonical session store and the central anaiero executable.
For a Cell Behavior instance called source-cells, first finish the session's
pending cycle and pause that engine. Then review and activate a pinned plan:

    anaiero session pause STORE source-cells pause-cells-01
    anaiero session migration-review STORE source-cells migrate-cells-01 CEVCELLSTATE/2 reviews/cells-migration.plan
    anaiero session migration-activate STORE source-cells reviews/cells-migration.plan
    anaiero session migration-status STORE migrate-cells-01
    anaiero session resume STORE source-cells resume-cells-01

The plan output location must be a new artifact path with an existing parent
directory. Review is read-only with respect to the engine state; activation
rechecks the pinned before and after digests and old state revision. Exact
accepted retries use the same operation ID and preserved plan bytes.
This is a **stopped/paused-session checkpoint conversion**, not a live
native-binary replacement. The host writer lock prevents a competing owner.

## Reproduce the functional integration fixture

From repository root, with native Windows/Linux C++20 toolchain:

    cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON -DANAIERO_ENABLE_MESH=OFF
    cmake --build build/native --config Release --target anaiero_engine_migration_tests
    ctest --test-dir build/native -C Release -R '^anaiero_engine_migration$' --output-on-failure

The SDK fixture creates a synthetic SDK counter, trains and pauses it, reviews a counter.checkpoint/1 to counter.checkpoint/2 transformation, rejects stale/tampered requests, commits the update, replays the exact operation ID, resumes learning, and recovers the updated state from canonical storage. It exercises an interrupted pre-ROOT migration and exact retry. The existing modular engine tests additionally convert a Cell Behavior checkpoint to the new schema, reject a damaged integrity tag, restore it, and continue cell behavior. This is functional verification, not endurance qualification.

## Follow-on CE-SW-43 increments

1. Add deployment-level semantic migration capable of changing module/build identity, with complete scheduled/lifecycle identity updates and explicit compatibility checks.
2. Implement declared reverse transformations or protected pre-migration restoration. An incompatible rollback must refuse; it cannot discard post-upgrade accepted work.
3. Generalize production migration parameters and compatibility version negotiation for other engine families. Retain the explicit reviewed bounds and immutable plans.
4. Add an integration test for the operator command invoking the production Cell Behavior migration on an actual persistent modular session.

## Restrictions

Migration is only permitted while paused and with no pending canonical operation. A nondeterministic transformer cannot activate its reviewed result. Precommit exceptions and failed guards do not advance the Projector. No claim is made about biological fidelity, transferable learning, physical intervention, or arbitrary binary upgrades.
