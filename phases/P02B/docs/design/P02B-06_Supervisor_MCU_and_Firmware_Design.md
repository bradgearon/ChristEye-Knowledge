---
title: "P02B-06 Supervisor MCU and Firmware Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/design/P02B-06_Supervisor_MCU_and_Firmware_Design.docx`  
> **SHA-256 (original DOCX):** `a2f6383678a0d02c2c4f127910ec75f0282e435c87e354dae2028690b1f82ed2`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Owner:** named in RACI; **Edition:** 2026-11-15; **Release level:** read-only research prototype.

## 1. Scope and boundaries

Candidate board A-B1 is a dry-zone, read-only status concentrator. Phase P02B uses a software twin; P02C is intended to use a NUCLEO-H743ZI2 development board with integrated STLINK-V3E and manufacturer-supported interfaces. The devkit is not a PCB production release. It must not touch electrode contacts, fluid lines, pump controls, or incubator setpoints. \[S-ST1\]\[S-ST2\]

## 2. Connections and power

Use the vendor development-board USB and ST-LINK connection for bench debugging. Separate the PC debug path from any future environmental signal path; no custom shield is released until ground, shield termination, EMC, isolation, cable strain relief, and rated power supply are signed. Candidate data connection: USB to host for prototype, wired Ethernet for later read-only status if required. USB enumeration loss must not affect independently qualified care. Final pin maps and logic levels depend on the chosen OEM adapter.

## 3. Firmware architecture

Boot with output permits absent. Report immutable board_id, hardware_revision, firmware_digest, calibration_digest, clock_domain, and permitted metric list. Accept board-observed samples with monotonic sequence and time fields. Put outgoing records into a bounded queue; flag queue overflow, missing calibration, watchdog reset, and serial framing loss. No default-to-last-value semantics: mark stale or unknown. Maintain an internal heartbeat that does not imply biological viability.

## 4. Messages and security

The Python reference uses canonical JSON with HMAC-SHA256 per simulated board, schema anaiero.p02b.telemetry/1. Distinguish this digital test from a production secure boot or key store. Hardware transport must prevent replay and bound record sizes. Only declared metric names are accepted; payloads with actuator keys are quarantined. Factory production must replace demo test keys and verify firmware provenance.

## 5. States and faults

BOOT → OBSERVE_ONLY; any invalid authenticated receipt or fault produces FAULT_LATCHED. An independently reviewed acknowledgement can clear the *software* latch. Loss of power, stale clocks, missing sensors, and host restart cannot authorize an output. Actual life support continues through separate qualified OEM controls. \[S-P02\]

## 6. Verification

P02B tests cover authentication, stale/future and duplicate events, undeclared metrics, missing units, nonfinite values and read-only rejection. P02C must add UART/USB loopback on inert loads, power cycle, watched reboot, cable disconnect, clock-drift characterization, EMC and instrument-vendor signoff before any external device hookup. Record sample rate and worst observed latency empirically, not from the MCU clock rate.

## References and status

**Status:** P02B software simulated or P02C candidate; no fabrication or biological actuation. Primary references: S-ST1 \<https://www.st.com/en/evaluation-tools/nucleo-h743zi.html\>, S-ESP \<https://documentation.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/product-overview.html\>, S-AD \<https://www.analog.com/en/products/ad5940.html\>, S-MEA \<https://www.mxwbio.com/products/maxone\>, S-NWB \<https://www.nwb.org/\>, S-OMERO \<https://www.openmicroscopy.org/omero/\>, S-ISSCR \<https://www.isscr.org/basic-research-standards/stem-cell-based-model-systems\>

