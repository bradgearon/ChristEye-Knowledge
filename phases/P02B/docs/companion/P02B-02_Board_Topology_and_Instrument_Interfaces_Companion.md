---
title: "P02B-02 Board Topology and Instrument Interfaces Companion"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/companion/P02B-02_Board_Topology_and_Instrument_Interfaces_Companion.docx`  
> **SHA-256 (original DOCX):** `72b48a4ce4b91a85f6cd96f8cd870b3af8c46a28253a0763a089a29b13e446aa`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

## Bandwidth is the first filter

The current vendor reference offers 1,020 simultaneous channels at 20,000 samples/s. With assumed stored 16-bit samples, raw acquisition is 1020 × 20,000 × 2 = 40,800,000 bytes/s (40.8 MB/s). This assumes 2-byte storage, not a confirmed vendor packing scheme. One-hour uncompressed storage is ~146.88 decimal GB; a day ~3.525 TB. Add optical frames, overhead, raw archive, metadata and fault buffers. This stream cannot be routed over BLE, nor should the small MCU act as the waveform recorder. Keep the vendor acquisition host and high-speed wired storage path. Each additional simultaneous instrument adds its own independent throughput; plan practical margins based on measured performance. \[MaxOne: https://www.mxwbio.com/products/maxone \]

## Separation of clock domains

Each event needs device_sample_index, device_clock, host_monotonic_ns, optional hardware_trigger_id, and a calibrated mapping uncertainty. Never mix wall-clock timestamps from a laptop, camera receipt time and acquired sample time as though equivalent. A physically shared TTL marker or vendor-supported hardware sync must be characterized for latencies and jitter; PTP is suitable only on interfaces that actually support its timing behavior. Report p50, p95, p99 and maximum observed end-to-end delays and missed events. Do not invent a neural eligibility-window timing threshold.

## Shielding and analog interference

Microvolt-level signals are not tolerant of casually placed switching regulators, pumps, wireless transmitters or ground loops. Separate headstage analog/reference circuits from control/communications; use vendor-reviewed grounding and shield terminations. Test with the care equipment operating and changing state. Treat EMI as a measured artifact, not a signal of neural behavior. RF diagnostic boards should be physically separated from the sensitive instrument, and BLE must never be the safety channel.

## Safety channel and trust chain

LOTUS/H proposes; the independent L evaluates; only a vendor-qualified instrument executes. There must be no alternate path through a development console, microcontroller bootloader, wireless gateway or debug cable. L rejects expired permits and operates when H reboots. Incubator care is a separate qualified OEM service, with a validated contingency that cannot be switched off by a new AI policy. Logs must preserve requested versus actually delivered actions.

## Board engineering proof package

For each candidate board: block diagram; electrical schematics; power tree; connector map; isolation/EMC review; clock design; firmware capabilities; secure update/recovery; simulated faults; test fixture procedure; board revision; signed calibration; software/firmware bill of materials; verified prohibition on physical actuation where applicable. Any undefined critical rating blocks fabrication release.

## Why the AFE is not a lactate sensor

An AD5940 measures electrochemical voltage/current/impedance for compatible sensing elements. It does not become a specific lactate meter by itself. A separately validated lactate-selective transducer, sampling geometry, matrix calibration and laboratory reference assay are required. \[Analog Devices https://www.analog.com/en/products/AD5940.html \]

**Sources:** MaxLab Live API https://api-docs.mxwbio.com/index.html ; NWB https://nwb.org/ ; original P02 A11–A15 gates. All rates and sensor limitations are proposal-specific unless vendor referenced.

## New evidence trace in v6

This companion is a scoped scientific interpretation. Distinguish instrument measurements, delayed task evidence and mechanism attribution. Model corrections in Formal Models and Reconciliation (source S-AUDIT) take precedence over historic scalar Crystal claims.

