# CE-SW-03 experimental software schemas

These schemas document **proposed diagnostic and interchange records** for ChristEye Anaiero Colony Intelligence. They are JSON Schema Draft 2020-12 design artifacts, not the canonical on-wire binary representation or commissioned biological automation.

| Schema | Contract |
|---|---|
| [colony_state.v1.schema.json](colony_state.v1.schema.json) | Revisioned, source-linked colony/participant snapshot with staleness and quality |
| [board_capability.v1.schema.json](board_capability.v1.schema.json) | Board capability advertisement, permitted readouts and interface inventory |
| [bread_meat_plan.v1.schema.json](bread_meat_plan.v1.schema.json) | Digital Bread/Meat learning plan OR explicitly non-executable molecular research dossier |
| [guidance_proposal.v1.schema.json](guidance_proposal.v1.schema.json) | Human-review recommendation backed by measurements, uncertainty and evidence |

The design is described in [CE-SW-03](../../docs/software/CE-SW-03_colony_intelligence_bread_meat_backplane.md). A valid schema is **not** evidence of lab validation, safe intervention, secure packet authentication or durable delivery. Stream framing, content hashes and cryptographic identity are CE-SW-01 runtime responsibilities.

## Verification expectations

Use a Draft 2020-12 JSON Schema validator, plus negative tests for missing source identity, invalid status enums, stale data incorrectly marked live, unknown permission classes, mismatched revisions and malformed digest values. Conformance tests must use canonical binary fixtures before this becomes a wire ABI.

No automatic molecular production, modification of culture care, physical Crystal consumption, neuronal stimulation or autonomous experimental intervention is authorized through these records.
