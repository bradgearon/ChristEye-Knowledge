# P02B read-only supervisor digital twin

The package `anaiero_p02b/` implements synthetic board-telemetry validation, authenticated frame receipts, duplicate/stale/forged message rejection, fault latching, append-only hash chaining, and separately signed scientific handoff requests.

Run:

```bash
python -m examples.p02b_supervisor_demo
python -m unittest discover -s tests -v
```

**No hardware actuation or biological inference.** Measurements are synthetic. HMAC here demonstrates a data-integrity pattern, not a complete production device identity/security implementation. Hardware board bring-up and safety interlock testing remain P02C gates.

See the [scientific director companion](modules/P02B-01_Scientific_Direction_and_Cell_Production_Companion.md), [supervisor MCU design](modules/P02B-06_Supervisor_MCU_and_Firmware_Design.md) and [hardware overview](hardware.md).
