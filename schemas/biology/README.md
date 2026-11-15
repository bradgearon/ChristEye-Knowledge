# Anaiero living-science data schemas (not actuation)

Revision 2026-11-15. The ten schemas define *observation, lineage, physical-candidate and governance data* for the P02B/P03+ research architecture. All experimental state remains read-only in Anaiero's software prototype.

| Record | Purpose |
|---|---|
| [CellBirthEvent](cell_birth_event.schema.json) | observed source-line division, not fabricated unique DNA ID |
| [CellIdentityObservation](cell_identity_observation.schema.json) | cell versus clone versus observation track |
| [AxonProjection](axon_projection.schema.json) | distinct structural/statistical/functional/task edges |
| [SpinalRootSubstrate](spinal_root_substrate.schema.json) | proposed physical spinal Root identity and qualifications |
| [FunctionalRootAssessment](functional_root_assessment.schema.json) | delayed functional performance and candidate substrate associations |
| [SoilRegion](soil_region.schema.json) | organoid whole, regional maturity and 3D coverage limits |
| [ChemiBusObservation](chemibus_observation.schema.json) | sample/analyte/form and uncertainty |
| [SustiBusCareStatus](sustibus_care_status.schema.json) | essential support and external qualified-care receipt |
| [ChangeProposal](change_proposal.schema.json) | PI/cell foundry/limits review for growth/food/peptide changes |
| [PhysicalCrystalCandidate](physical_crystal_candidate.schema.json) | unvalidated content-carrier hypothesis, explicitly not installable |

**Meaning:** JSON Schema constrains input **syntax**, not biological truth. A value entered in an evidence field is not automatically a validated scientific observation. Unique cell-instance IDs are database observation IDs. A lineage barcode is not a globally unique number assigned to each daughter cell. No genomic editing, nutrient formula, peptide dose or live physical actuation is encoded in these schemas.

[Controlled process library](../../processes/README.md) · [research evidence](../../research/evidence/anaiero-biology-2026-11.md) · [WDS license](../../LICENSE.md).
