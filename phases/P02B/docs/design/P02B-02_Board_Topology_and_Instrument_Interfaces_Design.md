---
title: "P02B-02 Board Topology and Instrument Interfaces Design"
project: "ChristEye Anaiero"
publication_date: "2026-11-15"
edition: "Complete Markdown edition of original DOCX"
classification: "P02B"
rights_holder: "We Develop Software LLC"
---

> **Complete text edition · 15 November 2026 · We Develop Software LLC**  
> **Original DOCX:** `phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/design/P02B-02_Board_Topology_and_Instrument_Interfaces_Design.docx`  
> **SHA-256 (original DOCX):** `efea27cb2986164b4d01b0e4df7ace99f12aee0c39c759070a2ac26988c47b70`  
> **Conversion:** Pandoc DOCX → GitHub Flavored Markdown. Original headings, tables, notes, and equations preserved where supported; images retained as assets.

**Status:** candidate Rev-A system architecture; NO board fabricated, NO mains design, NO electrode-drive circuitry. **Owner:** electrical/firmware lead + independent safety reviewer.

## 1. Physical partitions

**Dry computing zone:** PC/GPU, data storage, vendor MEA hub, Anaiero WindBus/Lotus, lab network gateway. **Instrument zone:** vendor-supported headstage/chip, approved optical path. **Environmental zone:** commercial qualified incubator/controller, independent sensors, authorized sample/feeding paths. New custom boards remain outside sterile liquid paths and cannot bypass vendor safety or approved incubator functions.

## 2. Board families

| **ID** | **Board / module**           | **Candidate compute**                                                      | **Inputs → outputs**                                                      | **Transport**                                  | **Authority**                                        |
|--------|------------------------------|----------------------------------------------------------------------------|---------------------------------------------------------------------------|------------------------------------------------|------------------------------------------------------|
| A-B0   | Host workstation             | x86/ARM PC + optional GPU                                                  | tasks, raw vendor streams, images → WindBus, NWB, reports                 | Vendor connection; wired Ethernet to NAS       | Software decisions only                              |
| A-B1   | Service supervisor MCU       | STM32H743-class dev board candidate                                        | calibrated status, discrete faults → event receipts, local logs           | USB debug + isolated fieldbus / wired Ethernet | Read-only in P02B                                    |
| A-B2   | Independent limit/interlock  | separate controller with watchdog + hardwired approved inhibit integration | out-of-band faults, permit states → independently verified inhibit/status | dedicated fail-safe signal path                | Independent veto only; cannot learn                  |
| A-B3   | Incubator telemetry adapter  | OEM environmental monitor / isolated MCU bridge                            | OEM temp/CO2/humidity/alarm output → timestamped observations             | wired fieldbus / vendor-provided port          | No new setpoint actuation                            |
| A-B4   | Chemistry sensor front end   | AD5940 eval board as optional generic electrochemical AFE                  | qualified sensor transducer → calibrated analyte samples                  | SPI to A-B1                                    | Measurement only; not intrinsically lactate-specific |
| A-B5   | Imaging/time trigger         | vendor camera/microscope with trigger logger                               | frame triggers/metadata → registered image events                         | vendor SDK / USB3 / supported Ethernet         | Observation only                                     |
| A-B6   | External diagnostics gateway | ESP32-S3-class candidate                                                   | filtered noncritical telemetry → secured BLE/Wi-Fi diagnostic stream      | Wi-Fi or BLE; host uplink                      | No safety or tissue-control privilege                |
| A-B7   | MEA interface                | MaxOne or chosen vendor instrument                                         | electrodes ↔ supported acquisition/stimulation paths                      | vendor API; proprietary connector retained     | Vendor-qualified only, no custom stim path           |

## 3. Firmware contracts

Every hardware board reports board_id, hardware_revision, firmware_digest, boot_nonce, capability_digest, calibration_ref, monotonic_counter, clock_domain, health_flags, fault_latch, and last_valid_sample_ns. The host MUST NOT infer a value from stale records. Use versioned CBOR/Protobuf or strict JSON during prototype; final serialization is a transport design decision. Require deterministic sequence and idempotent receipt handling, clock fit, and explicit unknown states. A command must include author, allowed operation class, request ID, target, expiry, and permit reference.

## 4. Physical electrical decisions not yet made

Unresolved before PCB: vendor-approved headstage ground/reference; actual host interface connector; available sync trigger; cable shielding and EMC; required galvanic isolation; rated fieldbus transceivers; service power architecture; emergency stop and supported inhibit; PCB temperature/humidity ratings; sterilization/contact compatibility. **Do not connect a custom DAC, amplifier, improvised relay, or RF gateway directly to MEA electrodes.**

## 5. Build order

A. Simulate A-B1 through A-B6 over WindBus and fault injection. B. Bench prototype MCU board, isolated USB debug, telemetry and logs with inert test sensors. C. Verify hardwired/vendor-approved inhibit on a cell-free test load. D. Integrate OEM environmental sensor output in read-only mode. E. Benchmark acquisition and time anchors on chosen real instrument without living material. F. Apply lab review before any biological/physical actuation.

## 6. Acceptance

Power-cycle recovery yields safe state; wired link interruptions do not replay requests; BLE disable does not affect care; unplugged PC leaves independently qualified care outside the learning policy; board-firmware signature verifiable; hardwired inhibit works if Lotus crashes; raw data stay on vendor/high-throughput path; debug controls cannot bypass safety. Acceptance values for time, EMI, drift and signal errors are fixed after bench characterization, not assumed from MCU clock speed.

**Hardware anchors:** STM32H7 ST https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html ; ESP32-S3 Espressif https://www.espressif.com/en/products/socs/esp32-s3/ ; AD5940 Analog Devices https://www.analog.com/en/products/AD5940.html ; MaxOne https://www.mxwbio.com/products/maxone ; MaxLab API https://api-docs.mxwbio.com/index.html .

## Implementation linkage in v6

The runnable reference lives in ../../src/anaiero_p02b/. P02B software permits read-only observations and independent simulated scientific handoff checks only. P02C electrical proof and P03 laboratory operations are separately gated in ../../registers/phase_roadmap.csv. This document remains a **candidate** until an owner signs the current vendor compatibility and qualification records.

