---
title: "WO-03 — Spinal Root, axon and functional connection evidence"
date: "2026-11-16"
status: "Specific observation and evidence work order; physical interventions require approved institution SOP"
owner: "Neuroscience, imaging, electrophysiology and independent review"
---

# WO-03 — Trace and qualify axonal and spinal Root evidence

**Purpose:** distinguish a possible spinal-derived tissue source, an actual observed axon, a statistical relationship, a functional influence and a task-related retained Root. **An axon is part of a neuron**, not a cell to clone separately. This work order operates on observed evidence; any physical source preparation or experimental manipulation requires separate approved procedures.

[Lab operations](ANIMON_LAB_OPERATIONS_v3.3.md) · [CE-A16 design](../specifications/v3.2/design/CE-A16_Animon_Spinal_Root_Design.md) · [scientific companion](../specifications/v3.2/companion/CE-A16_Animon_Spinal_Root_Scientific_Companion.md).

## 1. Prerequisites — no map without a source

| Step | Actual work | Required record | HOLD condition |
|---|---|---|---|
| 03-01 | Confirm authenticated prep/Soil/source/lot; region identity and actual source-qualified care | WO-00/01/02 source/care releases | unmatched lot or current source incident |
| 03-02 | State ONE question: structural contact, statistical association, directional influence, or learned-task contribution | declared primary hypothesis and edge class | a stain used as proof of all four |
| 03-03 | Select approved image/optical/electrical assay with instrument owner and method ID | source and target coordinates, calibration, sampling depth and clock | missing spatial/time reference |
| 03-04 | Define actual control regions and alternative electronic/environmental paths | comparison plan and source/recipient topology | shared input unexamined |
| 03-05 | Decide if observations are longitudinal or destructive | sampled region, method, preservation and repeatability | claiming same cell observed across methods without identity bridge |

Markers are **observational candidates** selected by the source model: spinal MNX1/ISL1/CHAT where relevant; neural MAP2/TUBB3; axonal neurofilament/MAPT/GAP43; candidate synaptic SYN1/DLG4 and glial phenotype. A marker signal does not by itself prove an axon transmits a specific learned fact.

## 2. Actual structural observation work

1. Retrieve the real cell-source documentation, qualified region map and image data from the approved observation method.
2. For each candidate neuronal soma, dendrite or axonal trajectory, record its actual source/target *region*, ROI, image series ID, spatial transform and depth/certified coverage. Use separate cell_instance_id versus axon_projection_id, with uncertainty.
3. Confirm the proposed structure across actual imaging planes and method capabilities, distinguishing apparent crossings in projected 2D images from potential 3D relationships. Mark hidden internal volume explicitly.
4. Preserve the **raw image and segmentation/tracing provenance**. A derived rendering alone cannot be used to locate an anatomical connection.
5. Check appropriate negative region/segmentation and source-specific identity controls. If a trace is ambiguous or source region unqualified, return STRUCTURAL_UNRESOLVED, not STRUCTURAL_CONTACT observed.
6. Link only the actually supported claim to a [typed AxonProjection](../schemas/biology/axon_projection.schema.json) and [RootEvidenceEdge](../schemas/animon/root_evidence_edge.schema.json). A synthetic DESIGN record is not an observation.

## 3. Functional follow-up (a different experiment)

A physical connection cannot be inferred from a pair of spike traces. The qualified neuroscience lead must identify actual source and target I/O, calibrated channel/spatial identities, reference and timing uncertainty, artifact/noise controls, common drive and permitted experimental study design.

| Actual information obtained | Supports | Doesn't support alone |
|---|---|---|
| repeatable morphology with source and destination | structural candidate | functionality or learning |
| correlated signals with valid clock and controls | association | directed axonal influence |
| independently reviewed source-target effect with applicable experimental controls | bounded causal hypothesis | semantic memory or individual Animon maturity |
| task-specific delayed reader under WO-04 with nominated physical contribution | candidate functional Root involvement | uniquely localized spinal engram |

**All actual neural interventions, electrical stimulation settings, genetic cell engineering and axon-growth manipulations belong to the lab's separate approved SOP and review; none is specified here.**

## 4. Yesterday / last qualified stage: what changed?

When the lab performs a legitimate repeated observation, the analyst compares:

- Same authentic source/lot, preparation, developmental state and coordinates; image field and resolution still match.
- Source-reported medium/care changes since prior observation; new health abnormalities or instrument recalibration.
- Observed track geometry and confidence across recorded sessions, with a registered coordinate system and known unobserved regions.
- Any actual earlier connection evidence versus current source/target recordings, with separate analysis of sampling and digital decoder changes.
- Whether a *task relation* changed, but only from a distinct WO-04 frozen reader and validated delayed task.

**If an axon-looking track disappears:** first check region registration, optical depth, segmentation, source identity, method, imaging artifact and care context. If the data remain valid, log STRUCTURE_CHANGED_CANDIDATE and request qualified review. **Do not command new neural growth or introduce peptides to restore the picture.**

## 5. Failure and stop matrix

| Problem | Record status | What the scientist does next |
|---|---|---|
| no actual spinal source identity | SOURCE_UNQUALIFIED | stop specific spinal claims, return to WO-00 |
| imaging depth unobserved | PARTIAL_COVERAGE | retain unknown interior, do not extrapolate |
| candidate axon absent under current optical field | UNRESOLVED or NO_STRUCTURE_DETECTED_IN_FIELD | document what was actually sampled, not global absence |
| two channels correlated but no control for common drive | ASSOCIATIONAL_ONLY | design independent study or qualify limitation |
| electrode/clock/registration changed | NONCOMPARABLE | require methodological bridge or new baseline |
| possible source-target effect without actual task | CIRCUIT_CANDIDATE | no Root acquisition or retention claim |
| performance exists only while external teacher connected | ASSISTED_USE_ONLY | run independent recipient-only analysis under qualified study |
| biological source or care invalid | HOLD_NEW_EXPERIMENT | preserve independent care; escalate to qualified facility |

## 6. Scientific release packet

The reviewer must receive:
1. Authentic prep/lot and source/regional phenotype, plus consent/ethics status.
2. Actual image and instrument raw files, hash/ID, clock, spatial transform and region coverage.
3. Discrete observed axon/cell/edge IDs with confidence; anatomical and statistical/functional claims typed separately.
4. Source-specific controls and known alternative routes, actual negative findings and a method-validity report.
5. Source-qualified care context; whether related task was actually tested or merely proposed.
6. Reviewer decision with supported boundary and successor study requirement.

When the evidence is insufficient, publish UNRESOLVED, not a new physical Root. Next [WO-04 retained task evidence](WO-04-learning-and-delayed-retention.md). Source precedents: [2026 corticospinal connectoid](https://pubmed.ncbi.nlm.nih.gov/42201810/) and [neural organoid study framework](https://www.nature.com/articles/s41586-024-08487-6).

© 2026 We Develop Software LLC. Patent document excluded.
