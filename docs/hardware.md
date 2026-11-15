# Hardware topology — Rev-A candidate boards

P02B specifies logical boundaries and *candidate* electronics, not final PCB manufacture.

| Board | Function | Candidate hardware | Boundary |
|---|---|---|---|
| A-B0 | workstation/host | PC, optional GPU | routing, storage, analysis and Crystal compilation |
| A-B1 | supervisor MCU | STM32H743 / NUCLEO-H743ZI2 class | read-only telemetry and watchdog |
| A-B2 | independent protection | separately reviewed limit controller | hard physical inhibit, not subject to Lotus |
| A-B3 | incubator service interface | OEM-supported gateway | read-only environment and service status |
| A-B4 | selected electrochemistry | AD5940-type evaluation front end | measurement only; requires qualified chemistry-specific transducer |
| A-B5 | microscopy and triggers | vendor optical system | instrument timestamps; reporter specificity |
| A-B6 | remote gateway | ESP32-S3/BLE/Wi-Fi | filtered diagnostics only, no actuation |
| A-B7 | neural acquisition | vendor HD-MEA | no custom electrode stimulation at P02B |

## Signal and routing rules

- Use wired, timestamped acquisition for bulk neural/optical streams; Bluetooth carries only compact status summaries.
- Every measurement has source ID, firmware version, sensor/calibration ID, units, local time/clock domain, uncertainty, freshness and integrity record.
- Physical interface pin assignments, ground ties, voltage rails, leakage/EMI, current budgets and inhibit paths are **unresolved hardware-specific releases**.
- Do not accept a missing value by substituting a nearby vendor's specification.
- On host loss or stale telemetry, preserve qualified essential care independently while suppressing unqualified learning actions.

## Bandwidth example

For a *hypothetical* stream of 1,020 selected 16-bit channels at 20 kHz, raw data are about 40.8 MB/s before metadata. This is arithmetic, not an approved sensor configuration. Long-term raw preservation, event compression and optical recording require separate budgets.

## Supplier materials

- [MaxWell MaxOne](https://www.mxwbio.com/products/maxone)
- [Analog Devices AD5940](https://www.analog.com/en/products/ad5940.html)
- [ST NUCLEO boards](https://www.st.com/en/evaluation-tools/stm32-nucleo-boards.html)
- [Espressif ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3)
