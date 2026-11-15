---
title: "P02B-09 Neural Acquisition and Multimodal Time Base Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/design/P02B-09_Neural_Acquisition_and_Multimodal_Time_Base_Design.docx`  
> **SHA-256 (original DOCX):** `5b95da0b416f8024d097f7133c0bcb41900bd52652682ed88ad4f25cb9bd4f61`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Owner:** named in RACI; **Edition:** 2026-11-15; **Release level:** read-only research prototype.

## 1. Vendor MEA boundary

A-B7 remains the vendor neural interface. Preserve manufacturer electronics, electrode references and permitted stimulation-disable mechanism; no custom electrode-driving PCB is authorized. Anaiero's new boards capture status, clock anchors and non-critical external sensors, not raw current injection. Bench waveform replay comes from inert simulation. \[S-MEA\]\[S-MEA-API\]

## 2. Throughput architecture

Reference throughput arithmetic: 1,020 channels × 20,000 samples/s × 2 bytes = 40.8 MB/s (decimal, before overhead) when this configuration is actually supported. Raw recordings travel by the chosen vendor high-speed wired interface to acquisition storage; BLE is limited to selected noncritical status. Provide queue watermarks and disk headroom, loss receipts and rollover checks. Compare measured sustained writes to actual recording payloads.

## 3. Clock domains

Record requested_time, vendor_delivered_time, MEA_acquired_time, camera_exposure_time, MCU_monotonic_time, and host_received_time separately. A captured physical trigger is preferable to inferred OS scheduling time for cross-modal alignment. Characterize offsets, drift, jitter and missing anchors; retain uncertainty rather than silently aligning everything to a screen timestamp.

## 4. Media and archives

NWB may represent electrophysiology, related stimulus events and experimental metadata; OMERO or equivalent stores optics under matched identity. Do not declare a single format's presence proof that every signal is simultaneously registered. Persist raw acquisition source, transform/decoder digest and exclusions with every RootEvidence record. \[S-NWB\]\[S-OMERO\]

## 5. Integration

P02B simulation acceptance: drop and duplicate external events, carry units/clock domain, preserve audit chain. P02C: vendor-supported device inventory, inert test-load recordings, measured physical inhibit and actual host link capacity. P03: actual lab preparation only after all upstream physical and ethical gates.

## References and status

**Status:** P02B software simulated or P02C candidate; no fabrication or biological actuation. Primary references: S-ST1 \<https://www.st.com/en/evaluation-tools/nucleo-h743zi.html\>, S-ESP \<https://documentation.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/product-overview.html\>, S-AD \<https://www.analog.com/en/products/ad5940.html\>, S-MEA \<https://www.mxwbio.com/products/maxone\>, S-NWB \<https://www.nwb.org/\>, S-OMERO \<https://www.openmicroscopy.org/omero/\>, S-ISSCR \<https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems\>

