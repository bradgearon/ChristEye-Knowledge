# CE-SW-09 — Learning/Crystal/RNA plan integration review

**State:** design integration notes on the v3.1 engineering line. Full compiled C++20 source, runnable manifests, fixtures, CMake changes and tests have been prepared in a separate repository-root `git am` patch; this GitHub branch does **not yet** contain that implementation.

## CEVPLAN/1

A machine-specific, immutable learning/training plan pins:

- `base_sha256`: the SHA-256 of the canonical sealed CEVLM/2 instance at which the plan starts.
- `artifact` declarations: a typed, SHA-256-pinned relative file for `digital_crystal`, `physical_crystal_candidate` (research-only), `digital_learning`, `rna_observation`, `support_profile`, or `visual`.
- `binding` declarations: each unique stage has exactly one explicit relationship to a Crystal, learning item, RNA observation, and support profile. Different stages may legitimately reference the same upstream source; hashes are identities, not inferred physical equivalences.
- `stage` declarations: an existing VLM2 process ID, unique task ID, duration, scheduling weight, stage visual and a synthetic action.
- `seal`: canonical SHA-256 of the unsealed descriptive plan.

The companion record headers are `CECRYSTAL/1`, `CELEARN/1`, `CERNA/1`, and `CESUPPORT/1`. A digital Crystal is not a proven biological memory carrier, and an RNA record never authorizes physical synthesis/delivery.

## Execution and reverse lookup

The reference C++20 session preflights the base digest, every artifact hash and role, each stage's process definition, and the pinned visual; it then advances the existing ProcessMachine one synthetic tick at a time. Every successfully committed tick emits a typed, post-commit progress receipt with source SHA-256 references, visual identity, the synthetic state hash, learning/RNA/support/neurite **proxy** counters, and `SIMULATED_UNCALIBRATED` origin. Callers can push receipts through the existing WindPacket evidence scheduler.

Because CEVLM/2 has an intentionally stable file grammar, the new saved machine gets a **separate CEVMAP/1 sidecar**. That file connects output machine SHA-256, base hash, plan hash, receipt-stream hash, and exact stage-to-Crystal/learning/RNA/support/visual mappings. It is not a scientific claim that knowledge was transferred.

## Verification gates

The prepared patch's C++ tests cover SHA-256 vectors, deterministic replay, CEVLM2 state restoration, plan seal tampering, duplicate bindings, bad base identities, changed artifacts and WindPacket delivery. Companion Python fixture checks cover the standalone file hashes, 12 progress events and the CEVMAP reverse index. GCC/Clang and ASan/UBSan passed locally. GitHub-hosted CI cannot be claimed until the implementation patch is applied to this branch and the workflow completes.

**Publication separation:** Keep the Pages-site branch independent. This PR is not a source-code merge, and no physical instrument or living-cell controller is authorized by the manifest design.
