---
title: "P02-01 Hardware Boundaries and Commissioning Companion"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/companion/P02-01_Hardware_Boundaries_and_Commissioning_Companion.docx`  
> **SHA-256 (original DOCX):** `7ca1439ae7285fb0af729dd56cc09f5086e34893376b29435a55562424440772`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Revision:** 1.0 \| **Status:** engineering rationale + unresolved vendor qualification \| **Mode:** cell-free

# 1. Why the instrument and living system have separate releases

An HD-MEA is an electrical observation/stimulation interface. Its successful acquisition of a reproducible waveform is an instrument result, not a learning result. Neuronal culture viability and synapse formation are different endpoints, and retention under a frozen decoder is stronger still. The P01 scientific claim ladder therefore remains: instrument competence → valid culture → acquired behavior → delayed retained behavior → joined contribution. P02 establishes only the first stage in simulation. Device feature labels and bundled API examples cannot be substituted for independent verification with the selected chamber and preparation. \[I01, I03\]

# 2. Electrode sites, readout channels and cell counts

The MaxOne reference lists 26,400 electrode sites and 1,020 simultaneous recording channels, so the nominal maximal observed site fraction is 1,020 / 26,400 ≈ 0.03864, or 3.86%. The switch matrix can select locations, but a location is not a unique biological cell, and one cell may influence multiple electrodes. The number of recorded channels cannot define starting cells per cartridge, distinguish single units without spike sorting, or establish full network observability. The published planar cell census from P01 is only relevant to its supplier-specific reference well, not an automatic MaxOne or 3D seeding recommendation. \[V01, I03\]

# 3. Throughput and clock decomposition

For N channels, sampling frequency f_s, and stored sample width b bits, uncompressed throughput is N × f_s × b/8 bytes/s. With 1,020 channels, 20,000/s and 16 bits: 40,800,000 bytes/s. Storage allocation must account for raw capture, derived spike events, masks, timing metadata, and optical data independently. End-to-end task latency is t_acquisition + t_transport + t_feature + t_decoder + t_policy + t_queue + t_delivery; neither mean GUI refresh latency nor a nominal sample period supplies these terms. P02 records the timestamp domains and an affine sample-clock fit, but makes no microsecond hardware timing claim.

# 4. The simulation is intentionally biologically implausible

SyntheticRecorder outputs a deterministic sinusoid plus seeded noise in arbitrary units. It does not emulate the spatial footprints of neurons, spike sorting uncertainty, calcium reporters, extracellular impedance, or stimulation artifacts. The simulated learner memorizes the reference task after repeated lessons; it models neither synaptic biophysics nor astrocytic plasticity. This narrow construction is deliberate: software tests need deterministic fault fixtures before biological variability enters. A future stochastic/dynamical simulator should be separately versioned and validated against real recordings, not silently substituted for the hardware qualification step.

# 5. Interface artifacts and pin-level safety

An electronics specification has three distinct abstraction levels: (a) protocol messages, (b) vendor API operations, (c) physical electrical outputs. P02 implements (a) and intentionally refuses (b)/(c). An eventual live build needs the actual supplier's electrode and reference-channel topology, connection diagrams, vendor-issued permitted profiles, charge and cumulative exposure constraints, signal ground isolation, and demonstrated inhibit path. We leave these as controlled unknowns rather than printing a pulse amplitude or wire assignment that might be unsafe for the eventual device. A software permit=true is not proof that an electrical inhibit works. \[V02\]

# 6. What would make the hardware path worth advancing

P03 candidate evidence includes a signed vendor compatibility record, measurable amplifier noise and artefact recovery, actual sample clock and delivered-event timestamps, a complete raw-data conversion test, and a documented cell-free inhibit test on a supported fixture. If a proposed instrument lacks a verifiable independent disable mechanism, the architecture is incomplete rather than eligible for an improvised relay. No P02 simulated performance should be used to infer human cortical, spinal or organoid capabilities. \[I01\]

# Additional engineering annex: procurement and proof-of-operation

**Platform-selection record.** Each proposed instrument configuration should enumerate host OS and CPU/GPU, vendor API build, array and chamber identifiers, readout channel limit, available synchronization endpoints, SDK data-export formats, supported hardware-inhibit mechanism, and vendor-approved accessories. Each claim must have one supplier specification or direct measurement and a version/date; a salesperson's informal compatibility claim does not close an interlock requirement. The project must compare MaxOne and a competing vendor only on the capabilities actually required by the first experiment: raw time-series access, stable channel identity, artifact observation, synchronized events, independence of the L veto, and continuous environmental support.

**Cell-free preflight report.** A P03 handoff packet shall contain: measured reference/ground noise before any culture; a sample of complete raw exported data with native units and sample index; successful open/close recording transaction; a simulated host termination while acquisition operates; an induced communication loss; channel-map export before and after a configuration change; and a demonstration that the vendor-supported inhibit prevents subsequent requested actions. A failure to provide a native delivered-event receipt leaves the crucial requested-versus-delivered distinction unresolved. A normal-looking oscillogram without verified calibration does not constitute an accepted interface.

**Design review questions.** Can another engineer reproduce the commissioning state from the manifest alone? Can every physical connector be traced to an approved drawing? What exact actions can still occur if the host PC crashes or restarts? Does the device continue qualified life support independently, or does it need a separate care controller? Is the planned optical path compatible with the actual electrode surface? Can the system export raw data even if the experiment's acquisition app is unavailable? Close each question with a signed observation rather than a guessed parameter.

**Nonclaim:** The reference's planar neuron/astrocyte well composition from P01 cannot specify an organoid's cell count or MaxOne plating density. Those decisions stay with the source partner and the P03/P06 preparation version.

# 7. Sources and evidence status

\[V01\] MaxWell Biosystems, MaxOne specifications, https://www.mxwbio.com/products/maxone (supplier). \[V02\] MaxLab Live API Overview v1.2.0, https://api-docs.mxwbio.com/index.html (supplier SDK, November 2026). \[V03\] MaxLab Live product page, https://www.mxwbio.com/products/maxlab-live. \[I01\] ChristEye Physical Learning Device Design v0.4, source archive. \[I03\] Anaiero P01 phase documents. Hardware quantities are supplier specifications or arithmetic; biological performance and compatibility remain unverified for Anaiero.

