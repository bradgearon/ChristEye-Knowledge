# BIO-02 · Axons, spinal Root tissue, connections and mapped use

**Owner:** developmental neuroscientist / electrophysiology lead. **Status:** detailed interface and evidence requirements, not a biological growth recipe.

## 1. Locked terminology

In Anaiero, **Root tissue** is proposed as the *spinal-derived physical foundational circuitry*. It can be a dedicated spinal module or a spinal-associated compartment in a larger Soil. It is not scientifically established that a spinal cord model stores all long-term knowledge; **Root function** remains the separately tested stable relation. The *Soil* is the living organoid whole.

A neuron has soma and processes. **Axon** carries neuronal output and can form terminals; **dendrites** principally receive and integrate inputs; **dendritic spines** are protrusions of dendrites. New synapses, axonal extension and additional progenitor-derived cells are three different growth processes. No "axon clone" or "spine cell" is a valid manufacturing unit.

## 2. Structural and functional map

| Layer | Observe | Candidate structures / molecular annotations | Interpretation boundary |
|---|---|---|---|
| Cell/region identity | nuclear/cell profile, spinal lineage | MNX1, ISL1/2, CHAT, VSX2, SOX9, ALDH1L1 | one marker alone insufficient |
| Axonal processes | registered longitudinal trajectories, branch and terminal location | neurofilament-associated profiles, MAPT/TAU; growth-associated GAP43 as a candidate | morphology not verified synaptic transmission |
| Dendritic organization | neurite arbor and candidate spine morphology | MAP2, actin, selected receptor sites | shape changes not proof of memory |
| Presynaptic organization | candidate vesicle release sites | Synapsin, synaptophysin, RIM/Munc13 | anatomical juxtaposition not proof of release |
| Postsynaptic organization | selected scaffold and receptors | DLG4/PSD95, SHANK, HOMER, AMPA/NMDA pathways | co-localization not a Root |
| Axon function | time-locked response, latency, routing, independence controls | membrane excitability and synaptic transmission | a shared stimulus can mimic connectivity |
| Support contact | astrocytic coverage and exchange | EAAT1/2, Kir4.1, candidate MCT transporters | metabolic association is not a teaching signal |

See [2026 corticospinal connectoid](https://pubmed.ncbi.nlm.nih.gov/42201810/) for actual examples of separated spinal/cortical units with structural and functional evidence, and [2025 sensory assembloid](https://www.nature.com/articles/s41586-025-08808-3) for functional multiregion observations.

## 3. Four typed Mesh graphs

Store independent edges: `STRUCTURAL_CONTACT` (imaging evidence), `STATISTICAL_COVARIATION` (recorded activity), `INTERVENTION_SUPPORTED_INFLUENCE` (validated manipulative contrast) and `TASK_RELATION` (declared learning). A `RootTrack` relates them by source/recipient compartment IDs; **it never automatically upgrades a correlation to an axon or synapse**.

Every `AxonProjectionRecord` needs `source_region_id`, `candidate_target_region_id`, `preparation_id`, `imaging_run_id`, `axon_track_ids`, `morphology_validity`, `measurement_resolution`, `functional_latency`, `control_id`, `reliability_interval`, `route_confidence`, `reader_version`, `valid_from` and `valid_until`.

## 4. Growth and joining decisions

**Stage A — Source/identity:** demonstrate the actual regional/spinal phenotype in the current preparation, not a motor neuron label transplanted from another cell system.

**Stage B — Structural:** independent imaging confirms a measurable axonal candidate course and receptor/terminal organization; no inference from electrode position alone.

**Stage C — Functional:** evidence of stimulus-to-response influence beyond shared-input, volume conduction and electronic route artifacts; record route timing and disruptions.

**Stage D — Learning contribution:** delayed task score under frozen decoder and no answer-bearing feedback; lesion/ablation-like tests are only discussed as controlled research questions, not described as fabrication instructions.

**Stage E — Protected expansion:** any network joining or morphology manipulation proceeds only after signed baseline and protection of earlier functions.

An **electronic bridge** is the first supported integration mode. Biological axonal joining is its own later laboratory qualification. Existing demonstrations of connectoids do not provide an Anaiero connection recipe.

## 5. Root storage ambiguity

A `SpinalRootSubstrate` may hold a linked `FunctionalRootRecord` if evidence supports contribution. Store many-to-many relationships: one functional Root can have multiple candidate contributing regions; one tissue region may contribute to multiple tasks. Maintaining a task does not require preserving the exact same cells forever, but renewal is not proven by matching behavior alone.

## 6. Food/peptide-change decision card

Any proposed trophic/receptor/ECM peptide change is a **scientific test proposal**, not an authorization to promote axon growth. The card requires actual regional identity, pre-intervention axon-track baseline, viability and protected-task performance, target engagement and off-target control, named human PI, lab SOP, and a future comparison that can distinguish route growth from generic excitability. **No active peptide selection, sequence, concentration or delivery method is inferred from the project metaphors.** Read [Sustibus/Chemibus](BIO-04-sustibus-chemibus.md), [growth governance](BIO-09-growth-control.md), and [evidence B02/B03](../research/evidence/anaiero-biology-2026-11.md).

© 2026 We Develop Software LLC · [License](../LICENSE.md).
