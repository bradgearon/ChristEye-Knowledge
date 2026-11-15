# WindBus and the central data router

WindBus connects hosts, StarChips and approved adapters through an auditable digital message contract. A **Wind** is always named with its sender, recipient, codebook, timescale, allowed information and evidential scope.

## Envelope example (software-only)

```json
{
  "schema_version": "1.0",
  "event_id": "example-0001",
  "source": "observer.digital",
  "recipient": "lotus.host",
  "module_id": "core-C-sim",
  "kind": "observation",
  "sequence": 17,
  "clock_domain": "simulation",
  "observed_at": "2026-11-15T00:00:00Z",
  "expires_at": "2026-11-15T00:00:05Z",
  "calibration_ref": "sim-cal-1",
  "payload": {"state": "UNKNOWN", "confidence": null},
  "authority": "advisory"
}
```

## Processing contract

1. Authenticate sender, declared route and version; reject unknown sources and forbidden recipient pairs.
2. Enforce monotonic sequence, deadlines, content size, clock uncertainty and idempotency.
3. Preserve requested versus delivered time and acknowledgements; do not silently replay queued physical commands.
4. Route observations to the Lotus decision queue; independent limits check any physical request outside the learner.
5. Append a tamper-evident receipt; mark missing and stale data explicitly.

## Separation of evaluator and learner

A training oracle may reveal an outcome **after** a trial response is closed. Sealed confirmation answers cannot appear in input messages, observer features, normalization windows or adaptive readouts. A frozen decoder must remain frozen for a biological-learning attribution.

## External connectors

Internet, Bluetooth, USB and LAN adapters deliver only authorized digital data. No public network endpoint is permitted to change feeding, cell growth, medium composition or electrode stimulation in P02B.
