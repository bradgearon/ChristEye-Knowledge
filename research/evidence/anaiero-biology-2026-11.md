---
title: "Anaiero biological systems — evidence anchors"
date: 2026-11-15
owner: We Develop Software LLC
status: "Cited reference index, not verification of a new Anaiero device"
---

# Scientific evidence and vendor precedence — revision 2026-11-15

Every process specification uses the **same named source IDs**. A literature study supports *its own preparation and assay*, not arbitrary generalization to a proposed Anaiero organoid. Read [P01 existing sources](../../phases/P01/references.md), the [corrected scientific synthesis](../full-markdown/R03_03_Formal_Models_and_Reconciliation_1_.md) and the [current field process index](../../processes/README.md) before choosing equipment.

| ID | Primary or scoped research | What it establishes / does not establish |
|---|---|---|
| **B01** | [Lo, Kiral & Park, *Functional readouts of neural organoids*, Nature Reviews Bioengineering (2026)](https://www.nature.com/articles/s44222-026-00507-6) | Electrophysiology, optical readouts and 3D coverage issues; not a demonstration of a general mind |
| **B02** | [Gibbons et al., *Human corticospinal organoid–slice connectoid*, Cell Reports 45, 117399 (2026)](https://pubmed.ncbi.nlm.nih.gov/42201810/) | Spatially separated cortical/spinal axons and measured connectivity; not Anaiero's own joined network |
| **B03** | [Kim et al., *Human assembloid model of ascending neural sensory pathway*, Nature 642 (2025)](https://www.nature.com/articles/s41586-025-08808-3) | Multiregion functional pathway with spinal, thalamic and cortical components; not interchangeable tissue modules |
| **B04** | [Morris et al., *CellTag-multi*, Nature Biotechnology 42 (2024)](https://www.nature.com/articles/s41587-023-01931-4) | Heritable lineage-barcode readout with transcriptome/epigenome correlation; not guaranteed unique IDs for every daughter cell |
| **B05** | [Leibovich & Goyal, *Limitations and optimizations of cellular lineage tracking*, PLoS Computational Biology 21 (2025)](https://pmc.ncbi.nlm.nih.gov/articles/PMC11996212/) | Barcode collision, dropout and false-lineage tradeoffs; lineage barcode ≠ persistent unique cell identity |
| **B06** | [*Morphodynamics of human early brain organoid development*, Nature 644 (2025)](https://www.nature.com/articles/s41586-025-09151-3) | Long-term optical observations of differentiation, actin/tubulin/nuclear organization; imaging changes interpretation and has limits |
| **B07** | [Suzuki et al., *Astrocyte–neuron lactate transport*, Cell 144 (2011)](https://pmc.ncbi.nlm.nih.gov/articles/PMC3073831/) | Rat hippocampal metabolic-support role in memory; not evidence that external lactate level equals a specific Anaiero Root |
| **B08** | [*Generation of spinal cord organoids ... lumbar fate*, Scientific Reports (2026)](https://www.nature.com/articles/s41598-026-45679-8) | Regional spinal organoid identity and physiology under a study-specific procedure, not a universal cell production recipe |
| **B09** | [ISSCR, *Guidelines for Stem Cell Research and Clinical Translation*, version 1.2 (2025)](https://www.isscr.org/resources/guidelines-for-stem-cell-research) | Reproducibility, source governance and ethics; local institutional review still required |
| **B10** | [ISSCR, *Stem Cell-based Model Systems*, section 4](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems) | Preparation-specific cell and model quality / reproducibility; does not certify Anaiero architecture |
| **B11** | [*The astrocytic ensemble acts as a multiday trace to stabilize memory*, Nature (2025)](https://www.nature.com/articles/s41586-025-09619-2) | Mouse context-specific astrocytic memory participation; transfer to a cultured organoid is an open question |
| **B12** | [*KIBRA anchoring PKMζ maintains persistence of memory*, Science Advances (2024)](https://pmc.ncbi.nlm.nih.gov/articles/PMC11204205/) | Scoped scaffold/kinase relationship in established animal paradigms; not a transferable packed memory |
| **B13** | [*Phase transition in postsynaptic densities*, Cell (2016)](https://pmc.ncbi.nlm.nih.gov/articles/PMC5564291/) | SynGAP/PSD-95 organization and condensate-like properties; not biological semantic bit storage |
| **B14** | [*BDNF-driven synaptic plasticity requires autocrine MMP-9 activity*, Science Advances (2025)](https://pubmed.ncbi.nlm.nih.gov/40991708/) | Local structural plasticity context; no universal MMP-9 on/off program or memory-specific peptide code |
| **B15** | [*Goal-directed learning in cortical organoids*, Cell Reports 45, 116984 (2026)](https://pubmed.ncbi.nlm.nih.gov/41720084/) | Bounded task response with limited tested persistence; not a general developmental curriculum |
| **B16** | [*The emergence of electrical activity in human brain organoids*, Stem Cell Reports (2025)](https://pmc.ncbi.nlm.nih.gov/articles/PMC12447339/) | Developmental diversity and network electrophysiology; spontaneous activity ≠ trained knowledge |
| **B17** | [*A human corticospinal organoid–slice connectoid* abstract and study details](https://www.sciencedirect.com/science/article/pii/S2211124726004778) | Biologically relevant long-range axon integration with measured structural and functional limitations |
| **B18** | [*Organoid analytical toolkits*, Nature Reviews Bioengineering (2026)](https://www.nature.com/articles/s44222-025-00384-5) | Optical/electrical/omics/biophysical measurement validity and sampling tradeoffs |

## Evidence fields required in every design

`claim_id`, `ordinary_biological_entity`, `source_id`, `preparation_version`, `assay`, `unit`, `observed_or_proposed`, `alternative_explanation`, `hypothesis_status` (F/X/H/D/A), `review_signature`, `revision`.

For a new source publication, cite its title/year/DOI in the **relevant paragraph**, not only at the end. Do not convert literature observations to local operating parameters.

## Technical correction register

- **[R03]** is authoritative for the project's mathematical reconciliation: logistic maintenance does not imply bistability; a slow state or candidate Crystal does not by itself establish memory; one performance score cannot separate storage versus access.
- **[P01]** original cell counts are a supplier-specific planar reference, not a 3D organoid inoculation or a "cells per StarChip" rule.
- **Cell ID** is a *data/lineage claim*, not an instruction to add a DNA barcode to every daughter cell.
- **Spinal Roots** is an Anaiero **proposed tissue architecture**; keep Root tissue and learned Root function separately recorded.
- **Physical Crystal and peptide learning** remain unvalidated; biochemical molecules are listed for identity, pathways, and testable readouts, not an executable biological update.

© 2026 We Develop Software LLC · [WDS Public Research Reference License](../../LICENSE.md). Patent documents excluded.
