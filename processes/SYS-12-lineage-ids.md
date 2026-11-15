# SYS-12 · Unique identifiers, clone lineage, cell track and digital Crystal copying

**Design problem:** a Crest should be able to trace all cells that descend from a qualified production population, but a birth-by-birth **unique identifier genetically encoded in every daughter cell** cannot be assumed from a clone barcode alone.

## Four identity layers

| Identifier | Scope | Formation | Durability |
|---|---|---|---|
| source_lot_id | a qualified source/production batch | provenance issuance | stable registry identifier |
| clone_lineage_id | cells inferred to descend from a shared ancestor | validated lineage observation or research barcode | shared across daughters; may be uncertain |
| cell_instance_id | single cell identified at a particular observation | observed cell-segmentation/identity event | observation-specific UUID |
| cell_track_id | inferred same cell across time or frames | trajectory association with confidence | can split, merge or become unresolved |

**CellBirthEvent** is only issued when division is actually observed or separately validated. Its fields: event_id, source lot, preparation/region, observed parent track, daughter observation IDs, time/clock, image support, segmentation/lineage uncertainty, assay provenance, reviewer and invalidation reasons. Never fabricate IDs to fill a gap.

## Molecular lineage barcoding — research candidate, not default

Heritable barcode approaches can link lineage to scRNA/epigenetic states, but collision, dropout, silencing and possible perturbation mean they do **not** ensure each dividing daughter receives a novel globally unique ID. [CellTag-multi B04](../research/evidence/anaiero-biology-2026-11.md) and [lineage tracing limits B05](../research/evidence/anaiero-biology-2026-11.md).

If the research team later proposes a biological identifier, the proposal must specify scientifically justified use, risk/consent/privacy, false match rates, genome perturbation assessment, assay readout and institutional approvals **before** it may become a physical protocol. This design intentionally does not contain editing vectors, delivery methods or barcode construct sequences.

## Cross-system identity

A neonatal cell is not a new “StarChip” by default. Record its cell/source identity in the *biological* registry, then link to candidate neuron/astrocyte/Root region. An electrode label (e.g., 1023) is not a permanent neuron ID; an image ROI may contain several cells; a molecular marker identifies a phenotype only within its method's limitations.

A digital Crystal clone has an immutable asset digest and target installation ID. These are byte-verifiable and fully distinct from a new biological descendant or transfer of learned biological memory.

## Change and maintenance

Version lineage claims and corrections. If cell observation IDs are retracted, dependent Root/axon graph edges must be flagged for re-evaluation without deleting raw evidence. Keep a graph of observation and provenance edges with explicit uncertainty and scientific reviewer signatures.

## Food/peptide, "encoded data in every cell" gate

The minimal safe **data** payload is not an inserted genetic code; it is a traceable external record: source/clone/preparation/cell-track/phenotype/observation context and declared method. Encoding additional bits biologically would be a separate high-uncertainty cell-engineering program with its own justification; none of the digital identity fields implies a new biological payload or a transfer of semantic knowledge.

[Birth and production process](BIO-01-crest-lineage.md) · [Soil development](BIO-03-soil-organoid.md) · [Growth decision review](BIO-09-growth-control.md).
