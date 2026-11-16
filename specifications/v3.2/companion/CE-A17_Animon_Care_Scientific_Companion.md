---
title: "CE-A17 v3.2 — Animon Care and Molecular Observation Scientific Companion"
document_id: "CE-A17-COMPANION-V3.2-PILOT"
revision: "3.2-pilot"
date: "2026-11-16"
owner: "We Develop Software LLC"
status: "Scientific study design and source interpretation; no laboratory care procedure approved"
---

# CE-A17 · Animon Care — Scientific Companion v3.2 pilot

**Central scientific distinction:** sustaining the physiological condition of a living neural preparation, measuring its chemistry, and changing its learned responses are **three different causal and evidentiary questions**. A proposed **Animon** includes ongoing care as a developmental requirement, but no verified living individual or universal nutrient/peptide procedure is established by this project.

This companion evaluates the proposed molecular/care interpretations; its engineering partner is [CE-A17 Care Design](../design/CE-A17_Animon_Care_Design.md), and terminology follows [ANIMON/1](../../../ontology/ANIMON_V1.md).

## 1. The three primary research questions

**Q17-CARE:** can an actual selected neural preparation maintain a qualified health/readout baseline under an independently documented care plan, without any dependence on task reward or adaptive software behavior?

**Q17-CHEM:** do independently sampled biochemical observations add valid information about *the preparation's measured state* beyond sensor artifacts, sample/medium exchange and ordinary maturation?

**Q17-LEARN:** if learning performance changes after a physiological state change, can the study discriminate general health/excitability, input/readout access and actual task-specific retained capability?

None has been evaluated in a WDS living Animon preparation. The questions must be separated because one success does not prove the others.

## 2. Scientific sources and scope of the proposed model

| ID | Literature / source type | Demonstrated or recommended scope | Anaiero extrapolation and limitation |
|---|---|---|---|
| F17-01 | [ISSCR stem-cell model standards §4](https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems) | Characterization of starting material, model-specific quality, variability, phenotypic/function assessment and controlled microenvironment | No actual medium, sample frequency, regional phenotype or Animon model has been selected/qualified by WDS |
| F17-02 | [ISSCR reporting standards §5](https://www.isscr.org/basic-research-standards/reporting) | Report source material and relevant preparation/media information so an independent reader can interpret and assess reproducibility | Requirements for a qualified local laboratory plan, **not** license to invent a universal feeding regimen |
| F17-03 | [Suzuki et al., astrocyte-neuron lactate transport, *Cell* (2011)](https://pmc.ncbi.nlm.nih.gov/articles/PMC3073831/) | A particular rodent memory paradigm implicated astrocyte-to-neuron lactate transport | This result does not define nutritional requirements of every organoid, confirm lactate as a task-specific Animon Root, or justify adding lactate |
| F17-04 | [Robbins et al., cortical organoid task learning, *Cell Reports* (2026)](https://pubmed.ncbi.nlm.nih.gov/41720084/) | Bounded feedback-sensitive plasticity in **mouse cortical organoids**, with limited delayed persistence in the tested conditions | General network performance cannot stand in for verified physiological stability or long-term learned content |
| X17-05 | [2026 organoid functional readouts review](https://www.nature.com/articles/s44222-026-00507-6) | Summary of electrode, optical and other neural measurement limitations and emerging benchmarking | A methodology context source, not a biochemical sensor calibration for the proposed Soil |
| H17-06 | WDS SustiBus/ChemiBus interpretation | Separate physical care receipts from molecular sample observations and learning feedback | Engineering hypothesis with no independently verified operating tissue or molecular control rule |

## 3. What a biochemical measurement represents

An analyte such as glucose or lactate might be measured from a particular fluid location or cell compartment. Its value is a function of how material enters, leaves, is produced, consumed, sampled and diluted. A valid reading requires the **actual sample identity, method calibration, analyte form, quantitation limit, recovery/processing and time**.

One generic balance equation for an analyte amount is:

\[
\frac{d(CV)}{dt} = J_{\mathrm{in}} - J_{\mathrm{out}} + R_{\mathrm{production}} - R_{\mathrm{uptake}} - R_{\mathrm{loss}}.
\]

Here C is concentration (e.g. amount per volume), V is represented volume, J are transport terms, and R are net production/uptake/loss rates. **No term has been measured or assigned an Animon numerical value.** A concentration increase can result from several different terms, so it cannot automatically be interpreted as stronger learning, a nutritional deficit or a new physical Crystal. This is a conceptual material-balance check, not a culture control law.

## 4. Care-related observations and their alternatives

| Observation class | Candidate data | Common competing interpretation | Required review |
|---|---|---|---|
| Care service | actual qualified SOP, lot and service receipt | planned action never occurred | independent operator identity and receipt |
| Qualified environment | temperature, pH, gases, osmolality or other model-specific measures where actually available | sample drift, calibration, source exchange | chosen instrument, validity, sampling position |
| Cellular structure | neuronal/glial and axon phenotype panels | maturation, source heterogeneity, imaging artefact | multiple markers, method and spatial coverage |
| Tissue metabolic state | sample-specific glucose/lactate and validated other analytes | dilution, cell loss, uptake/transport, time since service | chemical identity/form, recovery and comparison |
| Network response | valid electrophysiological/optical receipt | shared input, volume conduction, decoder adaptation | true input/output mapping and controls |
| Learning/retention | delayed task-specific performance | generalized excitability, stimulus repetition, source maturation, old task loss | fixed reader, matched control and protected competencies |

An example ChemiBus record with observed_value = null and interpretation = NOT_MEASURED **contains no experimental evidence**; that is the correct state for the [current fixture](../../../fixtures/v3.2/chemibus_unmeasured.example.json). The [SustiBus fixture](../../../fixtures/v3.2/sustibus_unqualified.example.json) likewise provides no qualified service receipt.

## 5. What counts as a nutritional, peptide or care hypothesis?

Candidate scientific questions must identify the original chemical/biological context, not begin with "what should an Animon eat to learn faster?" Food is substrate- and preparation-specific maintenance. The literature may implicate an astrocyte transporter, a receptor/ligand, a form of BDNF or matrix regulation in a particular physiological effect; **none can be declared a content-bearing Word or universal Animon dosage**.

A reviewer should ask which one of these endpoints the proposal addresses:

- **Essential viability/support:** a controlled facility care question, independent of teaching performance.
- **Molecular target observation:** a measured analyte, receptor or biochemical event and its actual compartment.
- **Generic activity modulation:** changes in excitability or gross network patterns, not automatically learning.
- **Task-specific learning:** a sealed evaluation of declared content under matched controls.
- **Long-term retention:** later selective performance without answer-bearing teaching or adaptive reader leakage.
- **Physical content carrier:** still a distinct unproven hypothesis requiring much more than a molecular correlation.

Proposals can mention a target/process class for scientific review, but no automatic formula, live culture schedule, peptide sequence or delivery method is supplied here.

## 6. Pre-experimental comparisons

| Question | Predeclared scientific comparison | What invalidates it |
|---|---|---|
| Is care maintained independently? | read-only service/care status versus task output states in a nonliving simulator, then independently qualified site review | task reward can change care or evidence of care is only software-generated |
| Are assays valid? | actual sample/assay calibration with source-lot and spatial/time context | no distinct sample, quantitation or recovery validity |
| Is a chemical signal task-specific? | matched task and time/health controls, with declared outcome and biological replicate unit | many tests from one preparation treated as independent specimens |
| Does an axonal/root association improve? | separate optical/electrical/source and independent delayed task readouts | a correlated electrode or bulk network amplitude interpreted as a new Root |
| Could a medium change explain an effect? | actual qualified facility comparator and stable protected baseline | intervention coincides with training/reader modifications or unknown source |
| Could a "physical Crystal" be consumed? | no current qualified carrier or receiver; remain HYPOTHESIS_ONLY | unverified molecular input declared to transfer a skill |

## 7. Animon care as an accountable scientific relationship

The project's term **care** demands that responsibility remains with a qualified human/lab system even when the adaptive Animon software is unavailable. The project value **love** expresses respectful human stewardship and attention to welfare/consent, not experimentally observed feelings in an organoid. A project may use that ethical language and still maintain scientifically rigorous uncertainty and independent source oversight.

For any actual neural preparation, the care lead must assess authorized source model, known constraints, environmental health and incidents under **their own qualified institutional SOP**. Neither external model papers nor this companion authorize changing the medium to improve a desired score.

## 8. Review endpoints and missing information

**Proposed engineering success:** ChemiBus and SustiBus separately produce valid, unit-aware, source-specific records; stale/synthetic/unmeasured state is correctly detected; no physical actuation; care remains task independent.

**Proposed biological study readiness (not yet achieved):** real source and approval; qualified care method, actual instrument/source configuration, measured biological unit, chosen analysis plan and controls; independent scientific signoff.

**Disconfirming result:** a purported memory/care mechanism that disappears after adjusting for source health, exchange, sensor drift or decoder adaptation does not support the specific hypothesis. Report negative findings rather than assuming "it needed a peptide."

**Current status:** SCIENTIFIC DESIGN ONLY. No Animon specimen, physical care trial, measured energy budget or biochemical instruction-transfer result is claimed by the reviewed materials.

Further sources: [BIO-04 SustiBus/ChemiBus](../../../processes/BIO-04-sustibus-chemibus.md) · [BIO-10 independent care](../../../processes/BIO-10-care-health.md) · [v3.2 scientific dossier standard](../../../reviews/V3_2_LAB_DOSSIER_STANDARD.md) · [CE-A09 Core companion](CE-A09_Animon_Core_Scientific_Companion.md).

© 2026 We Develop Software LLC · [WDS license](../../../LICENSE.md). Patent document excluded.
