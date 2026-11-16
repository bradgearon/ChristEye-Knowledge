---
title: "WO-00 — Human cell-source, genetic identity and material release"
date: "2026-11-16"
status: "Actionable procurement/QC work order; specimen processing requires approved lab SOP"
owner: "WDS scientific PI and qualified cell-foundry lead"
---

# WO-00 — Select, receive, authenticate and release biological source

**Goal:** obtain a documented, legally and scientifically qualified source for proposed Animon Soil and possible separate spinal Root research. **This work order does not authorize collecting donor tissue, reprogramming cells, manipulating genomes, or physically processing a cell culture without the responsible institution's approved SOP.**

[Program standard](ANIMON_LAB_OPERATIONS_v3.3.md) · [BIO-01 Crest](../processes/BIO-01-crest-lineage.md) · [CE-A09 Core](../specifications/v3.2/design/CE-A09_Animon_Core_Design.md).

## 1. Required decisions before obtaining biological material

| Step | Scientist must actually do | Record and release evidence | HOLD if |
|---|---|---|---|
| 00-01 Scientific PI selection | Name laboratory and PI; choose a precise preparation objective: qualified neural/Soil model and separate spinal-derived source hypothesis | institution ID, PI, ethics/material review ID, hypothesis | no authorized research site or primary question |
| 00-02 Source type | Select qualified banked **human iPSC line(s)** or an alternative source documented by the selected institution. Identify primary independent lines and model objective | source class, source institution, species, bank accession, planned batch hierarchy | source chosen by generic label without identity or allowed use |
| 00-03 Ownership/rights | Obtain and review material transfer terms, consent limitations, use/export permissions and donor privacy controls | lawful source certificate/MTA version and restricted donor identifiers under institution policy | transfer/export/usage limitations unresolved |
| 00-04 Model method | Select a named peer-reviewed *source-specific* organoid/spinal method and an institution-approved SOP version before any physical procedures | controlled method reference and PI/lab approval | no verified method or local process owner |
| 00-05 Replicate design | Define independent source lines, lots, individual preparations, batches and repeated technical assays separately | experimental-unit map and preliminary comparison/uncertainty plan | cells/recording channels incorrectly counted as independent preparations |

**Default:** banked, approved, authenticated source; no direct human donor sampling. The program's "Crest" is a *provenance and source governance system*, not a new technology that creates genetically labeled baby neurons by default.

## 2. At material-intake day: check certificates and custody

| Item | Concrete receipt | Release acceptance | Owner |
|---|---|---|---|
| Bank/line/lot | bank accession and original lot; physical manifest barcode, received time and authorized receiver | documents match material and consent; mismatch → quarantine under institutional SOP | cell-foundry lead |
| Source history | source tissue/origin class; donor-independent public ID; passage/population doublings and bank state | exact source lineage and permitted use known; conflicting records → HOLD | PI + source steward |
| Transport/receipt | source-certified conditions and recorded exceptions, any received package integrity issue | deviation reviewed by source/care lead; not silently accepted | lab operator |
| Genetic identity | documented source STR/SNP identity profile and method/version where appropriate | match approved bank reference **per laboratory acceptance criteria**; uncertainty → source HOLD | genomics lead |
| Genomic integrity | appropriate karyotype or CNV/structural variant QC report and known genotype when relevant | approved source/stage interpretation available, abnormal finding adjudicated | genomic scientist |
| Culture integrity | validated contamination panel (including model-appropriate mycoplasma check), authentication/viability and cell-state QC | all required validated tests meet source SOP, not a guessed universal percentage | cell-care QA |
| Source phenotype | origin-appropriate pluripotency/lineage marker panel with actual method and references | source confirmed by **multiple** meaningful criteria, not one marker | cell biology lead |

**If an item is missing:** mark SOURCE_UNQUALIFIED and do not promote biological claims. A source certificate is evidence of what was tested at a particular time and lot; it does not prove future cultures remain genetically stable or sterile forever.

## 3. What genetic material is actually collected, and why?

First identify which research question requires each sample. **Do not collect DNA or RNA simply to fill every field.**

| Sample stream | Intended evidence | What to preserve | How to handle a failure |
|---|---|---|---|
| Genomic identity/reference | line authentication, clinically/biologically relevant genotype, genetic stability | source-specific DNA assay certificate, reference profile ID, run/version/coverage and chain of custody | discordance → SOURCE_HOLD and request qualified genetics review |
| RNA expression/lineage | qualified neural/glial/regional marker *expression*, not an inherited memory | RNA specimen ID, actual preparation/region/stage, QC, assay batch, raw data and panel/reference atlas | inadequate quality or unmatched source/stage → invalidate this phenotyping claim |
| Protein/spatial phenotype | cell/neural/astrocytic identities, processes and spatial pattern | tissue image/assay region, marker names, imaging depth, method, localization and artifact assessment | marker presence alone cannot release a functional-neural claim |
| Medium/environmental chemistry | specimen-specific metabolism/health state | medium lot/actual sample origin and time, calibrated analyte/units, processing history | missing origin/calibration → NOT_MEASURED or INVALID |
| Observed birth/lineage | actual division and daughter tracks where independently recorded | parent/daughter observation IDs, imaging run/source and confidence | dropout/unobserved division → NO_BIRTH_CLAIM; never invent DNA IDs |

All **physical sample collection, isolation, preparation, storage and genetic assays** require **the selected qualified laboratory's actual SOP**. This document specifies *which result and record are needed*, not wet-lab recipes, reagent quantities, editing constructs or procedural incubation settings.

## 4. When to repeat quality checks

- **At source lot acceptance:** qualified identity, permitted use, relevant genetic/stability and contamination reports and source model.
- **At new source lot/line or major source change:** reopen genetic identity, microbiology, phenotype, and compatibility review using the laboratory's predeclared QC plan. **Do not inherit** acceptance from a different source lot.
- **At approved milestone sampling:** where the study requires developmental expression/genotype stability, record stage/region/sample/certificate and method-specific QC. A later sample is not comparable to an early one if cell population or assay changed without planned analysis.
- **Whenever an unexpected phenotype appears:** separate possible source misidentification, assay error, genetic instability, mixture/contamination, ordinary maturation and environmental factors. **Do not modify the culture in response to software prediction.**
- **At release of any science claim:** confirm the source/lot and relevant QC were valid at the time of the actual observation, not only before experiment initiation.

## 5. Crest per-cell identifier policy

- Record source_lot_id and clone_lineage_id (shared ancestry with method uncertainty).
- For an actually observed cell, assign a cell_instance_id; an inferred continuous trajectory has cell_track_id with validity and confidence.
- A cell-birth event requires an observed division/qualified evidence, never a synthetic timestamp.
- A barcode inherited by daughters **does not provide a fresh globally unique genetic ID to each daughter**. Engineering such a mechanism is a separate regulated/approved cellular research proposal, not a default Crest process.
- Molecular profiling does **not** directly read a stored "book" or the semantic content of a living Root.

## 6. Release and exception algorithm

~~~text
IF source lot/permission or laboratory owner missing → HOLD_SOURCE
ELSE IF authenticated identity conflicts → QUARANTINE_SOURCE under local SOP
ELSE IF contamination/genome/phenotype QC outside actual SOP → SOURCE_REVIEW
ELSE IF required assay/source metadata missing → INCOMPLETE_SOURCE_QC
ELSE → submit SOURCE_RELEASE_CANDIDATE to PI + cell foundry + QA
ONLY actual signed facility decision → QUALIFIED_SOURCE_FOR_NAMED_STUDY
~~~

**Output file:** signed source register and source-release certificate, linked raw documentation; approved SOP references; exception log; study-specific source qualification. No real lot data is supplied by this repository.

**Sources:** [ISSCR model-system requirements](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems), [ISSCR reporting requirements](https://www.isscr.org/basic-research-standards/reporting), [Nature neural-organoid framework](https://www.nature.com/articles/s41586-024-08487-6). Next: [WO-01 Soil development](WO-01-soil-development-and-maturation.md).

© 2026 We Develop Software LLC. No patent document included.
