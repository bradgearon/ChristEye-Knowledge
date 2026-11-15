---
title: "P02B-09 Neural Acquisition and Multimodal Time Base Companion"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/companion/P02B-09_Neural_Acquisition_and_Multimodal_Time_Base_Companion.docx`  
> **SHA-256 (original DOCX):** `129335c6cce94c70fc462f88193ad7215916e98f9c910e7f3ac806e416a372e8`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Claim boundary:** the work is a software/electronics research design, not a validated wet-lab SOP or mechanism.

## Signals and concepts should not be collapsed

An electrode site is not a neuron; a sorted unit may drift; a fluorescence ROI is not automatically an astrocyte or a Root. A changing digital decoder can simulate learning even when neural dynamics are unchanged. Every study comparing acquisition or retention should freeze the reader or label an adaptive-readout experiment separately. \[S-CEDEV\]

## Synchronization math

At the simplest, fit t_ref = a t_local + b on observed anchors and report residuals, not only mean offsets. Nonstationarity, dropped triggers and different sampling windows may invalidate a single linear fit. Use physical event identities and timestamps from acquisition rather than assuming a neural response latency from operating-system receipt time.

## Evidence for learning

Performance above matched controls during training is an acquisition question. A delayed cue-only test with a frozen reader examines retention under the tested circumstances. Astrocytic features, RNA, proteins, spine movement and lactate might help explain mechanisms, but none may be promoted to task content without independent task linkage. Keep all assay durations and burden in the design.

## References and status

**Status:** P02B software simulated or P02C candidate; no fabrication or biological actuation. Primary references: S-ST1 \<https://www.st.com/en/evaluation-tools/nucleo-h743zi.html\>, S-ESP \<https://documentation.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/product-overview.html\>, S-AD \<https://www.analog.com/en/products/ad5940.html\>, S-MEA \<https://www.mxwbio.com/products/maxone\>, S-NWB \<https://www.nwb.org/\>, S-OMERO \<https://www.openmicroscopy.org/omero/\>, S-ISSCR \<https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems\>

