---
title: "P02-01 Hardware Boundaries and Commissioning Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/design/P02-01_Hardware_Boundaries_and_Commissioning_Design.docx`  
> **SHA-256 (original DOCX):** `f701672ae88d537840b41497a33def9457540b616d810cab82ccfc956a1ea51c`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Revision:** 1.0 \| **Status:** digital executable + procurement unknowns \| **Release:** P02 \| **Owner:** hardware / systems leads \| **Mode:** ZERO CELLS, NO ACTUATION

# 1. Build outcome and system boundary

P02 commissions the complete logical path H → C → E and the advisory paths C → O → H → L using simulated electrodes and synthetic recordings. This is a real software integration release, not an electrical fabrication release. No organoid, neurons, astrocytes, biological medium, custom electrode interface, fluid dispensing, or tissue stimulation is present. The single supported I/O implementation is SimulatorAdapter; the VendorReadOnlyPlaceholder refuses all recording and physical commands until vendor-specific compatibility is qualified. This separation is mandatory because a successful API demonstration does not validate electrical or biological safety. \[I01, I03\]

# 2. Topology and connection labels

The **dry host** runs H, Lotus policy, E, routing, audit, and analysis. A future **culture compartment** will contain the vendor-supported array, sample chamber and headstage; a future **service compartment** will contain qualified environmental instruments. In P02, these last two exist only as interface records and unresolved drawing requirements. A request leaving the host must go through H and L. L is independent of the trained controller; its P02 implementation can permit only simulated task cues, synthetic recorder reads, and digital scoring. The adapter does not expose a hidden physical command channel. See p02_architecture.png and role_and_wiring_matrix.csv.

| **Drawing ID** | **What the drawing must show**                                                      | **P02 evidence / remaining dependency**                                           |
|----------------|-------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------|
| M01            | Host, recording hub, culture and service partitions, physical access, strain relief | Logical partition diagram exists; measured hardware fit is U01/U06                |
| E01            | Ground/reference topology, instrument reference, cable shield, verified inhibit     | Vendor electrical drawing and cell-free fault study required before P03           |
| S01            | Sample clock, event clock, shared synchronization reference and timestamps          | Synthetic three-anchor fit passes; physical shared-marker measurement remains U05 |
| D01            | Data route from raw stream through validity mask, decoder, E and archive            | Implemented through typed records; vendor raw byte format is U07                  |
| F01            | Independent material feed/sample/waste identity and fault isolation                 | Document only; no P02 pump, reservoir or actuator                                 |
| B01            | Identity and geometry of any eventual living sample                                 | Inherits P01 allocation register; cannot be released by P02                       |

# 3. Reference instrument — exact scope

MaxOne's current published profile lists 26,400 electrode sites, a selectable set of up to 1,020 readout channels, 20 kHz sampling per channel, a 3.85 × 2.10 mm active region, and a device-compatible stimulation function. **Electrode sites are not independently recorded neurons.** A switch-matrix selection is a channel map requiring its own digest and version. The MaxLab Live API has Python and C++ components; the selected installed SDK version and permitted operating paths require supplier confirmation. Never infer stimulator limits, pulse waveforms, ground pins or connector wiring from the feature list. \[V01, V02, V03\]

# 4. Hardware abstraction contract

The abstract adapter record requires adapter_id, api_version, mode, source_units, channel_map_digest, clock_domain, raw_export_format, physical_output_enabled, and qualified_config_ref. P02 enforces mode ∈ {SIMULATED, READ_ONLY_PLACEHOLDER} and physical_output_enabled = false. SyntheticRecorder.acquire(count, start_ns) returns bounded blocks of four channels in simulated-arbitrary-unit, not volts; logical_cue() accepts only context, first, and second. The vendor adapter intentionally raises HardwareUnsupported. A later adapter must not inherit permission from the simulator; it needs a separately signed qualification and independently demonstrated disable path.

# 5. Commissioning sequence

1.  Freeze P01 reference task and versioned role assignments; confirm all U01–U11 unknowns remain visible.

2.  Create a deterministic synthetic signal fixture with seed, sample geometry, block hash, and expected outputs.

3.  Establish a monotonic host clock and simulated sample clock; fit anchor map and calculate residuals.

4.  Instantiate WindBus only with declared source → recipient → event family permissions.

5.  Start in OFFLINE; transition to READY only after configuration, reserve, watchdog, and clock checks.

6.  Run BASELINE without feedback, then TRAINING with an evaluator-owned label path, then EVALUATION with the readout frozen.

7.  Inject invalid routes, stale sensors, low storage, absent clock anchors, duplicate events, and host-loss faults.

8.  Confirm that physical commands remain impossible in every mode; archive evidence and sign the release report.

# 6. Resource and interface budget

The full MaxOne illustrative raw recording is 1020 channels × 20,000 samples/s × 2 bytes/sample = 40.8 MB/s, or 146.88 decimal GB/hour and 3.52512 decimal TB/day. The two-byte representation is a **planning assumption**, not a vendor-confirmed export format. Optical frames, metadata, compression, and continuous duty cycle are additional. The P02 mock uses four channels and 1 ms sample spacing, small enough for reproducible local tests, and must not be labeled a throughput benchmark for MaxOne. \[V01, I01\]

# 7. Acceptance and handoff

Pass the software-side A01–A10 gates from acceptance_matrix.csv: output lock, envelope validation, route ACL, fault receipts, clock fit, repeatable synthetic capture, evaluator isolation, immutable readout, independent veto and audit integrity. A11–A15 remain OPEN: measured timing, actual API qualification, validated independent inhibit, NWB export, and multiple-device physical synchronization. P03 procurement and laboratory commissioning are blocked until the relevant open items close. No P02 output is a biological learning, neural connectivity or chemical support finding.

# 8. Source anchors

\[V01\] https://www.mxwbio.com/products/maxone · \[V02\] https://api-docs.mxwbio.com/index.html · \[V03\] https://www.mxwbio.com/products/maxlab-live · \[I01\] CE-DEV-001 v0.4 §§5–6, 12–15, 21–22 · \[I03\] Anaiero P01 phase roadmap and document standard.

