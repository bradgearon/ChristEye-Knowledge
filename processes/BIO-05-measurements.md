# BIO-05 · Assays, RNA/protein observation, electrical activity and tissue growth

**Owners:** assay, microscopy, electrophysiology and statistics leads. **Purpose:** separate measured phenomena from the interpretation that a Root grew.

## Multimodal instrumentation plane

| Plane | Record | Specific instruments / candidates | What not to infer |
|---|---|---|---|
| Electrical | time stream, electrode map, artifact windows, latency, delivered cue | vendor HD-MEA or scoped patch-clamp assay | electrode ID = unique cell or axon |
| Morphology | field, z-depth, soma/neurites, possible dendritic spines, registration | qualified light-sheet/high-resolution microscopy and selected markers | visible branch = functioning synapse |
| RNA | gene/transcript identity, region/cell sample and batch, expression/localization | validated transcriptomics, RNA imaging or selected transcript reporter | watching every transcript in every cell continuously |
| Protein | proteoform, antibody/reporter specificity, compartment, assay uncertainty | selected proteomics, immunostaining, translation-associated assay | observing an engram "stored in the protein" |
| Metabolic | glucose/lactate, dissolved gases, pH, osmolality, sample location and calibrated reference | validated analyte-specific sensors or endpoint assays | concentration = transport flux or need for nutrients |
| Learned function | trial identity, physical I/O receipt, frozen decoder, delayed score and matched controls | independent task evaluator | success on Pong = generic intelligence |
| Lineage | source lot, parent/descendant probability, optical tracks, selected barcode readout | imaged cell division or separately reviewed lineage method | guaranteed unique ID for every daughter |

The strongest recent overview is [functional readouts of neural organoids (2026)](https://www.nature.com/articles/s44222-026-00507-6). [Longitudinal organoid morphodynamics (2025)](https://www.nature.com/articles/s41586-025-09151-3) demonstrates selected cell/subcellular observations over developmental time, with specific imaging limitations.

## Canonical observation fields

- Data identity: observation_id, source_run, preparation_version, source_line, region_id, candidate_cell_track_id and source modality.
- Calibration: instrument_id, firmware/protocol_version, feature extraction hash, channel/electrode map, reference control, sample recovery, detection limit.
- Timing: original acquisition time, clock domain, synchronization anchor, delay uncertainty, pre/post task relative offset.
- Biology: target species/form, compartment, developmental state, viability marker and covariate.
- Claim: data validity, raw asset digest, inference hypothesis, counterfactual, confidence bounds and reviewer signature.

Use **Neurodata Without Borders** or similarly self-describing scientific stores for acquisition lineage, keeping raw and derived data immutable. Do not substitute a model-generated confidence score for calibrated experimental confidence.

## Joining RNA/proteins to an identified Root

Example: a candidate cell region shows an activity-linked protein change and also a task response. This supports only association until a separately justified test distinguishes target-specific contribution from source-line maturation, generalized excitability, experimental handling, background activity, assay drift and temporal coincidence.

Record three linked but independently assessable entities: **(i)** named chemical/species event, **(ii)** the structural axon/synapse observation, **(iii)** delayed task-specific Root performance. If any is not measured, the join remains *UNRESOLVED*, not assumed true.

## Data QC states

VALID, PARTIAL_COVERAGE, CALIBRATION_STALE, CLOCK_UNALIGNED, BELOW_DETECTION, ARTIFACT_CONFOUNDED, UNRESOLVED. An invalid trial does not become a correct HOLD response. Any automatic explanation must cite measured fields.

## Food/peptide/growth card

Measured biological change is a prompt for qualified human review, not an actuator trigger. Require assay-specific substrate/form, real sampling position, health confounds, protected Root history, source/lot compatibility, independent scientist approval, and a clear measurable endpoint. No amount or delivery recommendation is created in software.

[Scientific evidence register](../research/evidence/anaiero-biology-2026-11.md) · [SustiBus/ChemiBus](BIO-04-sustibus-chemibus.md) · [BIO-09](BIO-09-growth-control.md).
