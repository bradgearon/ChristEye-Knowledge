---
title: "CE-A02 v3.2 — Animon Long-Term Retention Engineering Design"
document_id: "CE-A02-DESIGN-V3.2-PILOT"
revision: "3.2-pilot"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Digital reference and unexecuted biological research plan"
---

# CE-A02 · Animon Long-Term Storage and Functional Retention — Engineering Design v3.2

**Design decision:** an individual Animon is intended to **develop and retain capabilities**, but data storage and a trained living Root must be treated as different phenomena. CE-A02 makes that difference auditable. It defines **two implementation planes**: (1) real source-controlled digital information and task-evaluation software, and (2) a separate *proposed* biological retention and mechanism study whose claims require an actual qualified preparation.

Sources: [ANIMON/1](../../../ontology/ANIMON_V1.md) · [CE-A16 spinal Root evidence design](CE-A16_Animon_Spinal_Root_Design.md) · [v3.1 A02 technical record](../../v3/design/CE-A02_long_term_storage_Design.md) · [new scientific companion](../companion/CE-A02_Animon_Long_Term_Retention_Scientific_Companion.md).

## 1. Six independently assessed things often called memory

| Capability | Real carrier / evidence | Correct status | What it is not |
|---|---|---|---|
| Source retention | bytes, original event rights and hashes | implementable digital storage | biological experience |
| Retrieval | lexical or other indexed lookup with clear source | existing digital prototype | arbitrary learned comprehension |
| Digital CrystalPack replay | verified copy and permitted teaching episode ingestion | existing software reference | a copied engram or biological upgrade |
| Digital skill assessment | independent response and delayed task score | partial evaluator framework | a verified result without an actual held-out run |
| Proposed living retained Root | a particular living preparation's later task-specific performance | not demonstrated in this project | general intelligence or located molecular memory |
| Proposed physical Crystal carrier | bounded material with appropriate reader and recipient-only content-selective transfer | unproven research | an ingestible or installable program |

**Naming:** a functional Root is a learned relation demonstrated under a defined input, decoder and task—not necessarily a spinal neural object. The *spinal Root tissue candidate* is covered by CE-A16 and requires its own evidence independent of A02's score.

## 2. Actual public software and known limitations

The repository includes:

- [anaiero/memory.py](../../../anaiero/memory.py): MemoryBank stores permissioned UsageEvents and performs a **set-overlap lexical retrieval**; it is not a neural memory or semantic learner.
- [anaiero/crystal.py](../../../anaiero/crystal.py): creates permissioned **digital experience replay packs**, verifies SHA-256, checks source and export rights, clones files and replays events to MemoryBank. It explicitly disclaims biological transfer and indicates a later independent assessment is needed.
- [anaiero/roots.py](../../../anaiero/roots.py): macro recall for X/Y/HOLD classes, retention_index with invalid-denominator guard, and RootAssessment candidate/hold rules for protected older task deltas; these are **digital metrics**, not tissue records.
- [examples/run_digital_replay.py](../../../examples/run_digital_replay.py): reproducible nonliving source → manifest → cloned package → replay → lexical recall.
- [anaiero/lotus.py](../../../anaiero/lotus.py): digital-only action allowlist and protected-root veto.

**Do not overstate implementation:** a stored digital UsageEvent is an authorized data record. It does not establish a trained StarChip has generalized from the event, and the replay example is **not a measured biological learning result or a full delayed task experiment**.

## 3. Source/teacher/recipient/evaluator architecture

~~~mermaid
flowchart TD
  Source["Consent-scoped UsageEvent source"] --> Ledger["Source + permission and immutable digest"]
  Ledger --> Pack["Verified digital CrystalPack"]
  Pack --> Mem["Nonliving MemoryBank source retrieval"]
  Mem --> DigitalEval["Separated digital test and frozen-reader plan"]
  DigitalEval --> DigitalCertificate["Digital result: only tested capabilities"]
  Study["Unapproved Animon StudyReadinessPacket"] --> Prep["Actual living candidate — not yet qualified"]
  Prep --> Instrument["Qualified interface and source/recipient readout"]
  Instrument --> Frozen["Fixed codebook and sealed evaluator"]
  Frozen --> Root["Functional Root assessment: delayed relation"]
  Root --> Evidence["CE-A16 candidate spinal Root relation"]
  Evidence --> Review{"Independent scientific interpretation"}
  Care["Independent qualified care / SustiBus"] --> Prep
  Care --> Review
  DigitalCertificate -. "Not evidence of biology" .- Review
~~~

Every arrow on the biological side is a **proposed source/measurement dependency**, not a live trained Animon. The digital source and researcher must protect rights, avoid answer-bearing leakage, and accurately record when information was available.

## 4. Typed data contracts and real fields

| Entity | Schema or code source | Acceptance |
|---|---|---|
| Digital source event | [UsageEvent](../../../anaiero/core.py) | legal scope and training/export permissions retained |
| Digital CrystalPack | [CrystalPack](../../../anaiero/crystal.py) | manifest, asset SHA-256, version, readers and consent guard |
| Functional Root | [functional_root_assessment.schema.json](../../../schemas/biology/functional_root_assessment.schema.json) | preparation, frozen reader digest, immediate/delayed score, protected tasks and state |
| Physical candidate spinal Root | [spinal_root_substrate.schema.json](../../../schemas/biology/spinal_root_substrate.schema.json) | tissue source/region/geometry identity and actual assay evidence |
| Typed physical relationship | [root_evidence_edge.schema.json](../../../schemas/animon/root_evidence_edge.schema.json) | separate structure, statistics, influence and learning evidence |
| Research study review | [study_readiness_packet.schema.json](../../../schemas/animon/study_readiness_packet.schema.json) | principal question, actual source and role approvals before any study |
| Animon identity | [animon_spec.schema.json](../../../schemas/animon/animon_spec.schema.json) | digital reference does not claim a physical Animon individual |

[Functional Root not-observed fixture](../../../fixtures/v3.2/functional_root_unobserved.example.json) has null scores, no delivery receipt and a zero-filled **synthetic checksum placeholder**. It is NOT an actual reader artifact, a valid retained relation or a physical Root measurement.

An actual assessment would be joined by source preparation, codebook/reader checksum, actual event delivery, task/evaluator receipt, feedback-off interval, real delayed horizon, protected old task assessments and independent care context. **A root_id alone cannot qualify an observation**.

## 5. Candidate digital acceptance test matrix

| Test ID | Action in the existing digital software | Required result |
|---|---|---|
| A02-D1 | Ingest allowed source event and recall its source | stored event provenance and lexical retrieval exist |
| A02-D2 | Attempt unauthorized training or export | reject the event/action before packaging |
| A02-D3 | Generate a CrystalPack and verify it | manifest and experience-asset digests match |
| A02-D4 | Clone, change one byte, then verify | tampering is rejected; original immutable version intact |
| A02-D5 | Replay into a fresh MemoryBank | permissioned events reconstructed, with replay receipt |
| A02-D6 | Try to count successful replay as independent learning | reject this scientific conclusion without held-out assessment |
| A02-D7 | Use RootAssessment with protected old-task regression | HOLD rather than silently improve the score |
| A02-D8 | Use retention_index with weak immediate advantage over chance | return undefined/unqualified rather than an artificial retention value |
| A02-D9 | Attempt to convert digital CrystalPack into a physical carrier claim | no such handler or biological installation semantics |
| A02-D10 | Run with no living source and synthetic evidence records | no physical preparation or learning stage promotion |

These checks are software capabilities; specific empirical values must come from executed test fixtures and recorded outputs. A passing software test does not validate any neural substrate.

## 6. Proposed biological retention study gates — non-operational

The first study must answer a narrow question: **does a specific validated preparation demonstrate a task-related history effect that persists at a predeclared later time, beyond a matched control and frozen host reader?**

| Gate | What must be real and qualified | What is never inferred |
|---|---|---|
| ROOT-0 | source permission, physiological care, actual instrument and baseline | source paper or simulated result equals local result |
| ROOT-1 | physical cue receipt and fixed codebook/reader; comparator | frequency of spikes = content meaning |
| ROOT-2 | actual training exposure with feedback provenance | training instruction alone = acquisition |
| ROOT-3 | independent immediate performance and per-class baseline | overall accuracy alone = Understanding |
| ROOT-4 | feedback off and separately declared delayed evaluation | short-term persistence = indefinite memory |
| ROOT-5 | protected older tasks and quality/health context | absent old-task data = no interference |
| ROOT-6 | CE-A16 physical substrate hypothesis with separate anatomical/functional evidence | delayed test success localizes a memory to spinal tissue |
| ROOT-7 | reviewer status with confidence/negative alternatives | a positive pilot = a general intelligent organism |

No electrical delivery parameter, neuron training signal, media adjustment or cell manipulation is set by this document. An institution's independent SOP and scientific director own any actual study.

## 7. Readout metric definitions

For balanced reporting of a three-class task with labels X, Y, HOLD:

\[
M = \frac{r_X+r_Y+r_{\mathrm{HOLD}}}{3}, \quad
r_c = \frac{TP_c}{n_c}.
\]

Each n_c must be nonzero and count **valid trials** in that class; record invalid and excluded trials separately. A constant HOLD decoder does not demonstrate the relation if it fails X and Y.

A proposed chance-corrected retention ratio is:

\[
R = \frac{M_{\mathrm{delayed}}-M_{\mathrm{chance}}}
         {M_{\mathrm{immediate}}-M_{\mathrm{chance}}}.
\]

As implemented in [anaiero/roots.py](../../../anaiero/roots.py), R is undefined when the immediate advantage over the chance comparator is too small. It may be negative or exceed 1; it is **not a memory probability**. Always report absolute immediate and delayed results and their preparation-level uncertainty first. Do not set a numerical biological pass threshold without actual study data and an independent qualified statistician.

## 8. Access versus retention is not identifiable from one score

A bookkeeping model can express the observed task relation as:

\[
P_{\mathrm{observed}} =
P_{\mathrm{chance}} +
(K-P_{\mathrm{chance}})\,A.
\]

K denotes a hypothetical retained ability under adequate access, and A describes momentary ability to retrieve/express it under a particular cue, reader and physiological state. This product cannot identify K and A separately from one measurement. A change in cue, decoder, care or sensor may alter observed performance without any lost physical Root.

**Interpretation rule:** if the reader or input route changed, the result is **ACCESS_CONFOUNDED** until separately controlled. If the delayed test was never conducted, **UNRESOLVED**. If a valid task fails, report **NO_RETENTION_DETECTED in the tested condition**, not "Animon memory erased."

## 9. Biological mechanisms remain nominated candidates

Synaptic scaffold candidates (DLG4/PSD95, SHANK/HOMER), KIBRA/PKMζ retention-associated findings, selected BDNF/TrkB signaling and astrocytic transporter biology are sourced within particular cell/animal preparations. The [scientific companion](../companion/CE-A02_Animon_Long_Term_Retention_Scientific_Companion.md) analyzes why these findings do not supply a programmable material Word or a directly transferable Animon memory.

**Protein detection, 3D neurite density, neuron division, scaffold enrichment or lactate concentration are not a semantic codebook.** No peptide recipe or Crystal ingestion step follows from their presence.

## 10. Growth and care decision card

Suppose a delayed score changes. The review order is: verify digital source integrity and rights; check actual input/decoder and time; determine whether essential independent care or source identity changed; compare observed output against valid matched conditions; assess whether a specific biological capacity hypothesis remains warranted. A chemical or cell/axon growth proposal must be separately reviewed with a named preparation, target/mechanism and protected older skills. It cannot be triggered directly by a low score.

The operator can write **StudyReadinessPacket = DRAFT**; a live intervention requires independent facility authorization. SustiBus maintenance continues even when teaching and new evaluation are held.

## 11. Stage and release classifications

~~~text
DIGITAL_SOURCE_VERIFIED
  -> DIGITAL_REPLAY_VALID
  -> DIGITAL_INDEPENDENT_TASK_TEST (separate)
          OR
BIOLOGICAL_SOURCE_QUALIFICATION_PENDING
  -> INSTRUMENT_AND_CARE_REVIEW
  -> ACQUISITION_STUDY_REVIEW
  -> DECLARED_DELAYED_ASSESSMENT
  -> EVIDENCE_CLASSIFICATION
  -> SCIENTIFIC_REVIEW
~~~

Claims are released **per hypothesis and preparation**, never by "Animon's memory is installed." If the actual study is incomplete, retain **UNRESOLVED** and preserve original measurements and all negative data.

## 12. Lab-review handoff and linked documents

The scientist receives: [v3.2 study readiness schema](../../../schemas/animon/study_readiness_packet.schema.json), [unapproved packet example](../../../fixtures/v3.2/study_readiness_unapproved.example.json), source/care/assay records and a predefined independent analysis question. The project supplies **no** qualified source lot, live media schedule, stimulation protocol or true neural retention result.

**Related:** [CE-A02 scientific companion](../companion/CE-A02_Animon_Long_Term_Retention_Scientific_Companion.md) · [CE-A16 Root/axon design](CE-A16_Animon_Spinal_Root_Design.md) · [CE-A09 Core](CE-A09_Animon_Core_Design.md) · [CE-A17 Care](CE-A17_Animon_Care_Design.md) · [BIO-06 teaching](../../../processes/BIO-06-teaching.md) · [BIO-08 Crystal research](../../../processes/BIO-08-crystals.md).

© 2026 We Develop Software LLC · [WDS Public Research Reference License](../../../LICENSE.md). Physical patent document excluded.
