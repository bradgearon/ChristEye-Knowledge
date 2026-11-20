# Anaiero native responsibility-boundary refactor

**Branch:** `refactor/anaiero-native-responsibility-boundaries`  
**Stack:** PR #55 → PR #57  
**Status:** Source changes proposed; cross-platform native CI has not yet qualified this batch.

## Decision

Do not treat native service files as natural homes for wire-format parsing,
content-addressing, physical filesystem recovery or XML/JSON presentation.
Keep lifecycle and authorization decisions in domain services; move binary
representation and identity computation into codecs/evidence adapters and
physical I/O into platform-specific infrastructure.

All existing serialized byte formats, canonical operation identities,
transaction sequencing, and store ownership semantics are intended to remain
unchanged. This batch does **not** introduce SQLite, pugixml, an alternative
canonical writer or a new cryptographic algorithm.

## Implemented source boundaries

| Existing area | Dedicated source | Ownership |
| --- | --- | --- |
| `native_store.cpp` | `native_store_filesystem.cpp` | Directory admission, file-node headroom, index coexistence inventory |
| `native_store.cpp` | `native_store_recovery.cpp` | Post-crash STAGING cleanup and obsolete derived-index reclamation |
| `native_store.cpp` | `native_store_canonical_codec.inc` | Exact root, transaction, blob, observation, record and receipt wire formats |
| `native_store.cpp` | `native_store_compaction_impl.inc` | Canonical-base construction and compaction |
| `native_store.cpp` | `native_store_recovery_impl.inc` | Indexed repair, replay, startup recovery and audit |
| `experiment_history.cpp` | `experiment_history_codec.cpp` and `experiment_evidence.cpp` | Frozen experiment formats, canonical receipt publication and typed engine/metric reads |
| `lifecycle.cpp` | `source_authority_evidence.cpp` | Immutable ownership record, SHA key, role fences and CAS guards |
| `mesh/service.cpp` | `mesh_record_codec.cpp` | Wire grants, pages, bootstrap, retention plans, cursor and identity hashes |
| `mesh/native_mesh.cpp` | `native_mesh_protocol.cpp` and `native_mesh_configuration.cpp` | Control/error wire format and native mesh config parsing |
| `deployment.cpp` | `deployment_record_codec.cpp` | Package/plan wire format and selected-package decoding |
| `modular_session_cli.cpp` | `modular_session_io.cpp`, `modular_session_application.inc` | Native operator file access and application composition |
| `mixed_campaign.cpp` | `mixed_campaign_codec.cpp`, `mixed_campaign_filesystem.cpp` | Campaign checkpoint format versus locking, bounded input and cache I/O |

**Compile boundary nuance:** `.cpp` adapters are separate compiled translation
units. The three `native_store_*.inc` files remain included in the original
storage translation unit so stateful functions can share `NativeStore` private
invariants without simultaneously redesigning its mutation and locking
ownership. This is a source-responsibility split, **not yet** fully independent
compilation of the stateful recovery/compaction methods. A later PIMPL/internal
state API migration can make that additional split without risking this
compatibility-preserving batch.

## Refactoring rules

1. Coders must preserve the full exact byte encoding for existing canonical
   and protocol schemas. Re-encoding a retained historical object must not
   change its digest or operation identifier.
2. The canonical store remains the single authority. A cache, projector,
   transport receipt or recovery helper cannot publish independent truth.
3. Post-interruption cleanup must verify ownership/root/generation fences
   before removing uncommitted files. Accepted blobs and current ROOT cannot
   be discarded as temporary objects.
4. A compiled codec must not take filesystem paths or TLS session handles.
   A domain service should operate on typed inputs and validated results.
5. Reusable SDK targets should not gain private platform headers or mandatory
   Python/XML/database dependencies.

## Functional verification

New `anaiero_native_store_boundaries` CTest exercises uncommitted staging
cleanup, accepted staging preservation, a malformed predecessor refusal and
obsolete index cleanup. The existing suites remain the important compatibility
gates:

- `anaiero_evidence_tests`, `anaiero_packed_index_recovery`,
  `anaiero_compaction_tests`, `anaiero_native_store_boundaries`
- `anaiero_recorded_experiments`, `anaiero_modular_session`,
  `anaiero_engine_migration`, `anaiero_engine_migration_cli`
- `anaiero_lifecycle_tests`, `anaiero_mesh_canonical`,
  `anaiero_deployment`, `anaiero_deployment_compatibility`,
  `anaiero_mixed_campaign`, `anaiero_modular_cli`

Example:

```sh
cmake -S native -B build/boundaries -DCE_WIND_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/boundaries --config Release --parallel 3
ctest --test-dir build/boundaries -C Release --output-on-failure
```

This documentation is **not a claim of passing CI**. Required Windows/Linux
native jobs, installed SDK conformance and exact migration/recovery fixtures
must be inspected at the proposed commit before merge.

## Residual debt — not silently declared complete

The lifecycle transition-enrichment path still contains inline observation
hashes and state recording. Mesh authorization still interprets a few private
owner/receipt formats. Deployment retained-package witness validation still
has low-level encoding checks. Some campaign maintenance and CLI operator
flows still construct evidence objects directly. They should be migrated to
typed evidence services in follow-up changes after the critical storage/mesh
contracts have passed regression checks. Merely moving a source file without
removing responsibility mixing does not close one of those gaps.

Similarly, the large legacy `native_session_service.hpp` is not automatically
bad architecture because of its byte count; its model-change orchestration
requires a separate service-ownership review before splitting it.
