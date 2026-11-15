---
title: "P02-03 Acquisition Clocks and Signal Pipeline Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/design/P02-03_Acquisition_Clocks_and_Signal_Pipeline_Design.docx`  
> **SHA-256 (original DOCX):** `456a18cdc2a940eadcefe1f2ba6d08db58319d570fe671fcec5ae598e0e52e01`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Revision:** 1.0 \| **Owner:** signal processing/data engineering \| **Software:** clock.py, acquisition.py, adapter.py \| **Mode:** synthetic only

# 1. Data path and truth of delivery

The acquisition service distinguishes (1) an intended task event, (2) the event actually delivered to a logical simulator, (3) a timestamped recorded block, (4) signal features extracted with a declared processing version, and (5) an evaluator's scored outcome. Those five records may disagree and must not be overwritten to look consistent. A learner's apparent failure can reflect an absent cue, channel-switch change, dropped buffer, clock misalignment or invalid feature; all are instrument failures until independently separated from learning. This split comes from the v0.4 observation contract and is required before studying Roots. \[I01\]

# 2. Synthetic block format

Each P02 SyntheticBlock includes block_id, source, started_ns, sample_period_ns, channel_count, a nonempty sample matrix, and unit='simulated-arbitrary-unit'. The first axis is time, second is channel. SyntheticRecorder(channels,seed).acquire(count,start_ns) produces bounded seeded fixtures and a canonical SHA-256 identity. The seed and acquisition version must be saved with the fixture. Signals are illustrative continuous values; the synthetic generator has no spike-detection physics, electrode impedance, capacitance, astrocytic state, or neural morphology. Test data must carry conspicuous SIMULATED classification in file names and metadata.

# 3. Clock domains and affine alignment

Three independent concepts must be recorded: wall-clock time (for human traceability), host monotonic time (for durations and deadlines), and device sample index/hardware timestamps (for electrophysiology). In P02 the simulator uses integer nanoseconds on a single host-controlled monotonic domain. fit_clock() takes at least three increasing (sample,t_ns) anchors and fits t_ns ≈ a + b·sample; it reports fitted ns/sample and maximum residual. The project config specifies an allowed residual for simulation only. Missing anchors, nonmonotonic markers, nonpositive slope or residual over the configured bound invalidate timing. An affine fit cannot correct arbitrary jitter, dropped frames or clock discontinuities; the record must preserve individual outliers and the exact fitting interval.

# 4. Reference signal routing

A valid adapter eventually must expose a versioned electrode/channel table, gain and scale factors, raw polarity, calibration date, sampling rate, dropout markers and the actual recording time interval. ChannelMap and feature-extraction versions must be frozen for the confirmation epoch. In a platform like MaxOne, 26,400 sites are selectable into up to 1,020 recording channels; recording channel numbers cannot be interpreted as immutable biological cell identities. Any switched route requires a new channel-map digest and a time-segment boundary. A change in gain can mimic a learning-associated response increase unless its acquisition provenance is recorded. \[V01, V02\]

# 5. Feature pipeline contract

For each observation, store raw_block_ref, source_module_id, sample_start/end, clock_fit_id, channel_map_digest, validity_mask, artifact_mask_version, filter/feature_version, feature_units, and feature_uncertainty. The preliminary digital P02 features are deliberately simple: block duration, count, per-channel mean, missingness and validity. Neural spike counts, bursts, evoked-response gains and astrocyte calcium are future modalities requiring independent detection methods. The first experiment must retain unfiltered intervals needed to check artifact-removal decisions, not only the final inferred labels.

# 6. Archival format and NWB mapping

The canonical P02 log remains JSONL for discrete events and simulation fixtures. The later interoperable ephys archive maps device + electrode geometry to NWB Device, ElectrodeGroup and electrodes, voltage traces to ElectricalSeries, and cue/stimulus markers to time-indexed stimulus/event records. ElectricalSeries uses volts and time as the leading dimension; therefore P02 arbitrary-unit samples must **never** be exported as though they were physical voltage. No validated .nwb exporter is included in this release. Implement and round-trip an export only after raw vendor units and metadata are known. \[N01, N02\]

# 7. Capacity, failure and quality gates

The illustrative raw MaxOne stream is 40.8 MB/s under the 1,020 × 20,000 × 2-byte assumption, before format overhead. For two modules it doubles. P02 tests a bounded queue and deliberately tiny blocks; capacity must be benchmarked on the selected host SSD before live work, including simultaneous imaging if intended. Record median, 95th, 99th and worst observed write latency, buffer overflow count, incomplete blocks, stale clocks and unexportable segments. Failed data are retained as invalid records rather than silently omitted. A P03 qualification must show actual source-to-disk throughput with headroom and fault recovery, not merely successful tests of this synthetic generator.

# 8. Specific verification

The supplied unit tests assert seeded block determinism, declared arbitrary units, channel-count bounds, monotonic clock anchors, fitted timestamp predictions, and rejection of high residuals. The P02 full demo writes a block hash and a verified audit chain. Open items: U05 physical sync, U07 real raw-format mapping, U08 full throughput, U10 electrode qualification. \[O01, O02, V01, I01\]

# 9. Source anchors

\[V01\] https://www.mxwbio.com/products/maxone · \[V02\] https://api-docs.mxwbio.com/index.html · \[O01\] https://open-ephys.github.io/gui-docs/User-Manual/Synchronization.html · \[O02\] https://open-ephys.github.io/gui-docs/User-Manual/Plugins/Record-Node.html · \[N01\] https://nwb-schema.readthedocs.io/en/latest/format.html · \[N02\] https://pynwb.readthedocs.io/en/stable/tutorials/domain/ecephys.html · \[I01\] CE-DEV-001 v0.4.

