---
title: "CE-SW-12 — SIM3 Persistent Multicolony Runner, Memory-Mapped Checkpoints and 3D Models"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
status: "Executable synthetic software prototype; no biological fidelity claim"
related: "CE-SW-01, CE-SW-07, CE-SW-08, CE-SW-09, CE-SW-11"
---

# CE-SW-12 — Persistent Multicolony Runner (SIM3)

## Scope

The purpose of SIM3 is to make a **small virtual living garden** restartable while preserving the synthetic cell lineage, network routes, resource state, activity/progress data and visual test artifacts. It is a digital surrogate, NOT a living culture, biologically valid growth simulator, genetic predictor, safe incubator-control system, infectious-agent model, or neuronal stimulation interface. The process names such as `foreign_stress` designate an abstract scalar disturbance, not a virus or disease with spread, replication, transmissibility or molecular mechanisms.

**Important integration boundary:** SIM3 pins an existing CEVLM/1 or CEVLM/2 source file **by SHA-256** but runs its *own explicitly declared 3D seed entities and processes*. It does **not** automatically reconstruct an arbitrary preexisting VLM2 journal or infer extra cells from its genome/source image; a full conversion adapter is a future milestone. The example generator accepts a real repository `.avlm` as the pinned source. A SHA-256 digest checks bytes, not whether those bytes constitute a scientifically accurate VLM model. The `CEVSOURCE/1` descriptor is a record of measurement availability, not raw genetic material.

## CEVRUN/1 file

```text
CEVRUN/1
name connected-gardens
source synthetic sim3/source-demo.txt <sha256>
colony crest vlm/process-garden.avlm <sha256>
colony blood vlm/process-garden.avlm <sha256>
seed crest 1 crest_progenitor 10 12 2 0
seed crest 2 blood_carrier 3 5 2 0
seed blood 3 neuron 20 17 4 0
link crest blood food 3
link crest blood data 4
link blood crest stimubus 2
process crest support 0 5 1
process crest divide 1 4 2
process blood teach 3 8 2
process blood foreign_stress 0 13 3
end
```

`seed` = colony, cell ID, conceptual kind, XYZ coordinates, parent ID. A child retains its `parent` and `birth` tick. All living and deceased cell records are kept for lineage auditing; when `energy=0`, the cell is flagged nonviable and gains a `death` tick. There is no destructive cell deletion or resurrection. Cell capacities and per-tick coordinates are bounded. Model constants are arbitrary, dimensionless *software rules*, not RNA/molecular or neural rate laws.

`link` = source-colony, target-colony, logical channel (`food`, `data`, `stimubus`), symbolic units/tick. `food` transfers a bounded conserved budget; `data` copies a synthetic learnable signal; `stimubus` is only a recorded symbolic route and **cannot drive electrodes**. The `process` family includes support replenishment, synthetic lineage division, synthetic teaching and a non-biological environmental stress proxy. All processes run deterministically at declared ticks or periods. No disease biology or foreign-agent optimization is present.

## State and persistence

The in-memory `Snapshot` contains all colony resource counters and cell states, including deceased lineage tombstones. **Every successful `step()` first retains an immutable `history/tick-NNNNNN.state` checkpoint**, then atomically updates a full `CEVRUNSTATE/1` latest checkpoint at `culture.state` through the repository's SW2.1 `durable::os::atomic_write`, and only then publishes the new in-memory state. This uses a host OS file lock to reject competing writers. On restart, a runner validates the pinned config and committed snapshot and resumes at the last successful tick, rather than reinitializing a culture.

A separate `MappedCulture` provides a read-only OS memory map of `culture.state`. It is a **view of one committed revision**: readers can keep that mapping while the writer atomically replaces the file with the next version. The map is not itself the durable log, and never authorizes writers to change committed bytes. The core already uses an atomic rename and file/directory sync adapter with Windows/Linux qualification caveats from CE-SW-11. It is not a substitute for transactionally consistent backup or verified disk power-loss tests.

Persistence limitation: SIM3 retains **one snapshot per tick plus a latest-state pointer**, but the history is not yet indexed as canonical SHA-256 WindStore objects and has no hash-chained commit receipts. Crash interruption between retaining a tick and advancing the latest pointer can leave an unreferenced history file; startup trusts only the latest pointer and rejects a conflicting history entry. SW2.2 should store each checkpoint by hash in WindStore and publish committed receipt links in its audit journal. A failed image export after state commit is reported as an I/O error but must not roll back or misrepresent the already committed simulated tick. `audit_disk()` checks the current latest state and corresponding historical checkpoint; full journal scrubbing and transaction-level migration are future work.

## 3D model and images

The renderer exports deterministic `tick-NNNNNN.svg` frames and Wavefront `.obj` geometry. Each cell is a tetrahedral marker with fixed coordinates and conceptual kind; dark markers indicate deceased cells. XYZ positions and parent-child lineage are explicit. OBJ lines include lineage and **logical connection wiring** between participating colonies, distinguishing digital circuitry from actual synapses. SVG is a 2D diagnostic projection including colors, labels, lineages and intercolony routes; its pixels are **not direct microscopy**.

Integration tests include committed, exact-byte **golden SVG and OBJ files** for tick 5, plus verification that each durable tick-history state matches the corresponding latest-state checkpoint. The tests compare output with these known artifacts and exercise isolated writer locking, persistent restarts, mapped snapshots across updates, source-hash mismatch, tampered checkpoint fail-closed, resource-dependent death tombstones, circuit routing, synthetic learning, and deterministic restart-vs-uninterrupted hash identity.

## Example CLI

Create a pinned configuration from an existing VLM image at the repository root:

```bash
python tools/make_sim3_example.py examples/vlm/process-garden.avlm \
  examples/sim3/source-demo.txt examples/connected-gardens.cev

cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

ce_colony_runner inspect examples/connected-gardens.cev ./run-state
ce_colony_runner step examples/connected-gardens.cev ./run-state 5
ce_colony_runner mapped examples/connected-gardens.cev ./run-state
ce_colony_runner obj examples/connected-gardens.cev ./run-state > ./garden.obj
```

The `source` descriptor can later describe an *approved* donor/research source, such as a blood, hair or engineered sample, but **one cannot predict the whole cell network or a learned engram from its sequence alone**. Store a consent-scoped opaque accession reference, acquisition method, measured phenotype, assay uncertainty, instrument profile and qualified study result. Do not commit identifiable genetic data to a public repository. A source hash is provenance, not a proof of biological capability or an identifier safe to publish without privacy review.

## Future SIM3.1

- Typed `CEVSOURCE/2` measurement and consent schemas; independent model-fidelity validation using real qualified datasets.
- Import actual sealed CEVLM/2 journal and all previous cell positions rather than only referencing its hash.
- True append-only WindStore snapshot receipts and per-colony subscription cursors for every stage.
- Configurable 3D/physics plugins with explicit model versions, spatial calibration and uncertainty, not invented biological kinetics.
- Secure multi-host peer routing after SW3 transport authentication and capability authorization.
- Actual camera/MEA observation data for the *viewer* only when approved and timestamped; neither sample/source hashes nor simulated actions imply biological control authority.
