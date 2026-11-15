# Anaiero molecular, cell and structure crosswalk — 2026-11-15

**Purpose:** a readable, engineering-facing map of *ordinary scientific entities* and their proposed roles in the living-computer architecture. This is **not** a shopping list or operational culture/feed/peptide protocol. All additions to living cultures require a specific qualified laboratory SOP.

| Biological scale | Marker, molecule or class | Proposed observed role | Usage class | Required evidence | Source IDs |
|---|---|---|---|---|---|
| source/lineage | SOX2 / PAX6 / NKX6-1 / OLIG2 | cell-fate and regional candidate identity | ASSAY / QUALIFIED_SOURCE | matched source assay; division and region evidence | B08/B09 |
| spinal phenotype | MNX1 / ISL1/2 / FOXP1 / CHAT | candidate motor-neuron classes in spinal tissue | ASSAY | multiple markers plus functional assay | B08 |
| spinal interneuron | VSX2 (CHX10), GAD1/GAD2, SLC6A5 | excitatory/interneuron and inhibitory subtype candidates | ASSAY | region-specific identity and transmitter function | B08/B16 |
| astrocytes | SOX9 / ALDH1L1 / GFAP / S100B | candidate astrocytic identity and maturation | ASSAY | multiple markers and cell function | B01/B08 |
| OPC/oligodendroglia | SOX10 / OLIG2 / PDGFRA | progenitor/oligodendroglial lineage state | LATER SOURCE | source-specific cell identity, myelination separate | B01/B08 |
| microglial candidate | P2RY12 / TMEM119 | immune phenotype when explicitly part of source | LATER SOURCE / ASSAY | sample identity and biosafety review | B09/B10 |
| axon morphology | MAPT/TAU, phosphorylated neurofilament candidates, GAP43 | axon identity/extension and growth-associated state | STRUCTURAL ASSAY | tracked axon course and functional recipient distinction | B02/B17 |
| dendrite and spine | MAP2, actin, tubulin | dendritic arbor, spine and cytoskeletal processes | STRUCTURAL ASSAY | morphometry and imaging artifact control | B06/B16 |
| presynaptic | Synapsin (SYN1), synaptophysin, RIM, Munc13 | release-site candidate organization | ASSAY | co-localization and separate transmission | B16 |
| postsynaptic | DLG4 (PSD-95), SHANK, HOMER, SYNGAP1 | synaptic scaffold architecture | ASSAY / HYPOTHESIS | selective structural contribution beyond density | B13 |
| excitatory signaling | AMPA / NMDA receptor families (GRIA/GRIN) | excitation and receiving properties | FUNCTIONAL ASSAY | recorded response with known receiving compartment | B16 |
| inhibitory signaling | GABA-A / glycine receptors; KCC2 / NKCC1 | reception and chloride-context dependency | FUNCTIONAL ASSAY | measured excitation/inhibition, not just markers | B16 |
| plasticity-associated | CaMKII, ARC, RhoA/Rac1/Cdc42 | selected activity and structural remodeling pathways | RESEARCH ASSAY | task-specific effect, receptor/axon and health controls | B12/B13 |
| retention hypothesis | KIBRA (WWC1) / PKMζ | context-specific maintenance candidate | RESEARCH ONLY | retention causality separated from access/viability | B12 |
| growth/reception | proBDNF, mature BDNF, BDNF prodomain | molecular forms and receptor-sensitive effects | RESEARCH ONLY | verified form, compartment, receiver and order | B14 |
| receptors | TrkB (NTRK2), p75 (NGFR), SorCS2 | BDNF-related receiver organization | RESEARCH ASSAY | receptor-state evidence, not assumed semantic Word | B14 |
| ECM regulation | MMP-9 / TIMP-1 | matrix and selected substrate processing | RESEARCH ONLY | local activity vs abundance, morphology and function | B14 |
| astrocyte metabolic transport | MCT1, MCT4, neuronal MCT2 | lactate transport in a rodent memory model | RESEARCH / ASSAY | sample source, temporal flux, extrapolation constraints | B07 |
| astrocyte ions/glutamate | EAAT1 / EAAT2 / Kir4.1 | glutamate clearance and ionic homeostasis candidates | RESEARCH / ASSAY | direct target function and tissue health | B01/B07 |
| astrocyte slow-state | selected adrenergic receptors / IGFBP2 | memory-associated slow states in certain mouse studies | RESEARCH ONLY | prediction beyond baseline; no portable content | B11 |
| complete medium | water / salts / bicarbonate / CO2 | qualified aqueous environment and buffering | QUALIFIED FORMULATION | supplier lot, local measurement, service receipt | P01/B09 |
| energy | glucose / pyruvate / oxygen availability | physiological metabolic support | QUALIFIED FORMULATION / ASSAY | measured support adequacy, not feedback per task | P01/B07 |
| biosynthesis | amino acids / glutamine / lipids / vitamins | support protein synthesis and membranes | QUALIFIED FORMULATION | complete supplier recipe only; no oral meal analogies | P01 |
| metabolite signal | lactate | production/uptake/exchange marker; may also be local energy carrier | ASSAY; NOT SEMANTIC PAYLOAD | sample location, volume, calibration, health and Root separation | B07 |
| carrier candidate | synaptic protein condensates / ECM / compact microcircuit | physical candidates for maintaining learned relations | HYPOTHESIS | specific content, physical carrier, compatible reader and causal tests | B13/B12 |

**Interpretation:**

- *Assay* means characterize an existing component, not feed it. *Qualified formulation* means a named supplier/approved culture-medium identity in the actual preparation, not a generic amount.
- *Research only* means no prescribed peptide or treatment: the study first has to establish molecular species/proteoform, receiving cell, compartment, control and safety.
- *Root tissue* is a **candidate spinal-derived component**. A Root's functional information is not directly read out from a marker stain.
- *Soil* is the organoid whole. *Crest* produces qualified source records; cell IDs and the rare proposed genetic lineage barcode must never be conflated.
- Molecular study of learning may be important without yielding an external installer for memories. A digital Crystal clone is not a biological clone.

## Full process links

[Source and lineage](BIO-01-crest-lineage.md) · [Axons/spinal Roots](BIO-02-axon-spinal-root.md) · [Organoid Soil](BIO-03-soil-organoid.md) · [SustiBus/ChemiBus](BIO-04-sustibus-chemibus.md) · [Measurements](BIO-05-measurements.md) · [Training](BIO-06-teaching.md) · [Growth](BIO-09-growth-control.md) · [Care](BIO-10-care-health.md) · [Science references](../research/evidence/anaiero-biology-2026-11.md).

**WDS research property © 2026 We Develop Software LLC.** See [LICENSE](../LICENSE.md).
