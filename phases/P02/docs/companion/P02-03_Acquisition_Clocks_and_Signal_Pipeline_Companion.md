---
title: "P02-03 Acquisition Clocks and Signal Pipeline Companion"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02_Cell_Free_Electronics_and_WindBus/docs/companion/P02-03_Acquisition_Clocks_and_Signal_Pipeline_Companion.docx`  
> **SHA-256 (original DOCX):** `a5e81608b8f2199c6c9e19c8689d43dd36c9dcc75e5dd994b985e06b10b7da6e`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Purpose:** Measurement validity, quantitative assumptions and what the synthetic signal does not establish.

# 1. Quantities and dimensional correctness

A sampled electrophysiology signal, if calibrated, is voltage in volts; a detected event is an occurrence count; a per-electrode firing estimate is counts per time interval. These are different quantities. A sample count is not an electrode count, and an electrode count is not a cell count. P02's arbitrary-unit sinusoid deliberately prevents false claims about neural recording amplitude. If future devices store integer ADC codes, conversion to volts needs the actual vendor scale factor and per-channel calibration. If those are unavailable, the archive must record native units and mark biological amplitude comparisons unresolved. \[V01, N01\]

# 2. Why three clock anchors, and what they do not solve

A basic affine model is t_i = a + b·s_i + ε_i for sample index s_i, true reference time t_i, slope b and offset a. With at least three anchors it is possible to estimate a line and inspect a residual rather than merely interpolate two points. But three well-aligned anchors cannot establish a small worst-case jitter between them; an actual device study must inspect many points across the declared clock interval. Clocks with drift changes, packet loss or discontinuities may require piecewise fits and independent hardware markers. The Open Ephys synchronization guide specifically emphasizes a shared digital timing reference for asynchronous streams; that is context for a later instrument, not proof that Open Ephys can directly drive MaxOne. \[O01\]

# 3. Acquisition bandwidth and recording policy

Bandwidth = channels × samples/s × bytes/sample. If the channel count is 1,020, each samples at 20,000/s, and storage width is 16 bits, then 40,800,000 B/s. Across a decimal hour: 146,880,000,000 B, or 146.88 GB; over 24h: 3,525,120,000,000 B, or 3.52512 TB. This number assumes no dropped channels, headers, data compression, event metadata or optical streams. An actual MaxLab file may not use the assumed 16-bit in-memory or stored representation. The selected duty cycle and claim requirements determine whether continuous raw archiving or targeted evaluation windows are necessary. Never discard the raw evidence needed for the particular claimed effect. \[V01, I01\]

# 4. Acquisition artifact versus biological event

Electrical stimulation can produce a measurement artifact temporally overlapping a neuronal response. Time alignment alone cannot distinguish them. A future experiment requires a qualified response window after measured artifact exclusion, sham controls, instrument-only recordings, and source-specific limits. The P02 mock has no electrode artifact model; it simply provides an input- and output-recording pipeline. A later calcium/optical stream has slower reporter kinetics and independent exposure timestamps, so frame receipt times must not be substituted for the moment of exposure. \[I01\]

# 5. SNR is not Understanding

An input channel can have clean spikes but no task discrimination. Conversely, useful information can be carried in distributed patterns that a per-channel magnitude test misses. SNR = mean event amplitude / baseline noise standard deviation is a valid selected-channel qualification definition with specified units and noise estimator, but high SNR does not establish task specificity, encoding content, or delayed Roots. A network connectivity inference also depends on common-cause controls, region sampling and the frozen evaluation method. \[I01, I02\]

# 6. The NWB choice

NWB defines structured extracellular-electrophysiology records including electrode tables, ElectricalSeries and timestamps. This is useful to share the data needed for independent re-analysis. The P02 phase supplies a mapping specification and schema, not a certified exporter. In a release with real raw data, the conversion must round-trip exact recorded samples or documented conversion losses, channel identities, validity masks, and event times. The build should retain checksums for both native and exported representations. A mislabeled unit silently destroys comparability even if the file validates as a document. \[N01, N02\]

# Additional engineering annex: timestamp and feature acceptance

**Required event record.** An observed response must carry at least: source equipment ID, preparation ID if relevant, recording epoch, channel-map hash, native units, raw data URI and checksum, exact sample range, requested event ID, delivered event ID if independently supported, clock-map version, artifact exclusions, feature-extraction version, and validity reason. This lets a second analyst recompute whether a response occurred at the claimed time. Missingness itself has a timestamp and source; do not fill gaps by copying the last normal value.

**Clock stress plan.** Generate synthetic anchors with known offset, known linear drift and controlled outliers. Fit the affine map; reject nonmonotonic anchors; inspect maximum and percentile residuals. Then introduce a clock step midway and verify the fit fails its declared quality gate or is segmented into two declared epochs. A mean residual alone can hide bursts of poor synchronization. On the selected hardware, the equivalent test must use captured physical events and must include both instrument acquisition clocks and host delivered-event records.

**Validity levels.** Label each block RAW_VALID, ARTIFACT_MASKED, TIMING_UNCERTAIN, UNITS_UNCALIBRATED, MISSING, or REJECTED, with an explicit reason. A channel's firing-rate estimate is unavailable if no valid detection exists; it is not zero. A calcium observation is a reporter measurement with spatial and temporal limitations rather than interchangeable spike data. Do not infer an astroengram from a calcium transient. Actual extracellular recordings and optical assays require distinct provenance and uncertainty models. \[I01\]

**Storage engineering.** Before P03, measure sustained and peak writes to the selected SSD at the target channel count, with file rollover, restart and disk-full scenarios. Record end-to-end delivery time and 99th percentile tail latency. Retain raw confirmation windows and the full configuration used to derive them. If a later export uses NWB, reconcile native sample counts, electrode labels, sampling rate and timestamps against the original files before deleting nothing. \[N01, N02\]

# 7. Source anchors

\[O01\] Open Ephys Synchronization, https://open-ephys.github.io/gui-docs/User-Manual/Synchronization.html. \[O02\] Open Ephys Record Node, https://open-ephys.github.io/gui-docs/User-Manual/Plugins/Record-Node.html. \[N01\] NWB spec, https://nwb-schema.readthedocs.io/en/latest/format.html. \[N02\] PyNWB tutorial, https://pynwb.readthedocs.io/en/stable/tutorials/domain/ecephys.html. \[V01\] MaxOne specs, https://www.mxwbio.com/products/maxone. \[I01\] CE-DEV-001 v0.4. Neither quoted interoperability nor physical synchrony has been demonstrated by the Anaiero P02 simulator.

