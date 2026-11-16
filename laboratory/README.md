# Animon v3.3 Laboratory Execution Packet

**Owner:** We Develop Software LLC · **Revision:** 2026-11-16 · **Status:** actionable research planning and electronic QA; physical instructions still require an authorized, validated laboratory SOP and qualified materials.

## Why this folder exists

Previous v3.1/v3.2 documents described architecture, candidate mechanisms and controls, but were insufficient as **scientist operating instructions**. This folder adds numbered work orders that state:

1. What the designated scientist must do or obtain.
2. What authentic source/genetic/cell/chemical/electrical evidence to collect.
3. What must be measured, with which source, stage, actual calibration and receipt.
4. What previous-day or stage-matched result to compare.
5. How to classify missing, invalid, noncomparable or critical observations.
6. Which changes/physical actions require a human-approved SOP and signature.
7. What completed evidence releases the next work order.

**Do not treat the work orders as an approved culture recipe or an autonomous biological system.** A real facility must attach its own current source-specific protocol and safe condition limits; invented numbers would be more dangerous and less executable than explicit required source values.

## Start in this order

| Step | Actual lab-oriented document | Output |
|---|---|---|
| 0 | [Source-derived phase/BOM](ANIMON_PHASE_AND_MATERIAL_BOM.md) | R0 digital, R1 planar reference (80k/20k), R2/R3 separate, P06 3D Soil UNSET |
| 1 | [Execution and daily operations standard](ANIMON_LAB_OPERATIONS_v3.3.md) | work-order structure, owner matrix, shift checks, daily comparison and HOLD logic |
| 2 | [WO-00 source/genetic identity](WO-00-source-and-genomic-qc.md) | qualified source/permissions, STR/SNP/genomic/contamination QC and release |
| 3 | [WO-01 Soil formation/readiness](WO-01-soil-development-and-maturation.md) | actual preparation identity, cell/glia phenotype, morphology and longitudinal evidence |
| 4 | [WO-02 daily support and ChemiBus](WO-02-daily-care-and-chemistry.md) | model-specific care receipts, measured analytes, limits and prior-day deviation decisions |
| 5 | [WO-03 axons and spinal Root](WO-03-axon-and-spinal-root-evidence.md) | actual source/target spatial and signal evidence, distinguished structural/functional/task edges |
| 6 | [WO-04 learning and delayed Root](WO-04-learning-and-delayed-retention.md) | actual task/cue/reader provenance, immediate/delayed evaluation, matched and negative controls |

## Forms, schema and software

- [Source-release form](forms/SOURCE_RELEASE_FORM.md) — fill at procurement/intake and update at significant source changes.
- [Daily shift and care worksheet](forms/DAILY_SHIFT_SHEET.md) — actual values, yesterday's comparable readings, approved SOP limits and disposition.
- [Deviation/change form](forms/CHANGE_AND_DEVIATION.md) — flags any physical care, molecular, source, assay, reader or neural research change.
- [Daily QC JSON schema](../schemas/animon/daily_qc_observation.schema.json) and [strictly nonbiological sample](../fixtures/v3.3/synthetic_daily_instrument_qc.example.json).
- [Read-only QC engine](../qa/daily_qc.py): compares recorded values and declares NORMAL, REVIEW, HOLD, INVALID, NONCOMPARABLE or UNRESOLVED. It **never controls the culture**.
- [CI tests](../tests/test_lab_workorders_v33.py) check work-order links, schema, the previous-day arithmetic and fail-closed behavior.
- [Execution backlog](EXECUTION_TASKS.json): actual work steps, owners, source prerequisite and held state. Assigned operators and dates are intentionally unresolved, not invented.

## Worked nonbiological daily-QC example

~~~bash
python qa/daily_qc.py fixtures/v3.3/synthetic_daily_instrument_qc.example.json
~~~

The sample is a made-up **reference instrument signal in arbitrary units**, not a glucose/lactate/organism measurement. Its example low/high limits are synthetic, not safe physiological values. Software checks the real value only when an authenticated laboratory supplies its approved bounds, correct source identity and qualified assay.

**Immediate next operational dependency:** select a qualified laboratory and concrete model/reference protocol, starting with P01's vendor-dependent planar example, before purchasing material or attempting biological operations. The current source already documents **80,000 neurons + 20,000 matched astrocytes per well** as an illustrative R1 2D count; this cannot be applied to P06 3D Soil.

## Linked source program

[Existing v3.2 four-module science](../specifications/v3.2/README.md) · [v3.2 lab-review standard](../reviews/V3_2_LAB_DOSSIER_STANDARD.md) · [P01 cell count source](../phases/P01/docs/01_Physical_Module_Composition_and_Counts.md) · [P01 care plan](../phases/P01/docs/04_Daily_Care_Quality_Gates_and_Assays.md) · [ISSCR model standards](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems).

© 2026 We Develop Software LLC. WDS Public Research Reference License. Physical patent file excluded.
