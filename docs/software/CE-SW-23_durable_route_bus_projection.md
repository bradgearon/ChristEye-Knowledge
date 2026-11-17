---
title: "CE-SW-23 — Durable Route WindBus Receipts and ColonyState"
project: "ChristEye Anaiero"
version: "0.015.x / SW6.1"
status: "Native digital-only local integration; secure mesh pending"
---

# CE-SW-23 — Delivering real simulated route events to the projector

The prior SW5 fork optimizer already commits digital World, route guidance and a pending event under one source ledger HEAD. This change implements the missing local downstream path: a distinct versioned **WindPacket change** message, durable WindStore event, SW4 weighted queue and **read-only ColonyState projector**. The optimizer remains a separate service.

## Components

| Layer | Responsibility |
|---|---|
| RouteReceiptDeliveryService | Orders events, journal-before-projection, controls local ACK, idempotent retry |
| RouteReceiptWindPacketCodec | Encodes 56-byte WindPacket header plus 96-byte route payload, full state SHA-256, schema 0xCE600031 |
| WindStoreRouteReceiptJournal | SHA-256-addressed packet objects, committed DIGITAL_ROUTE_RECEIPT events; conflicts rejected |
| RouteRuntimeProjectionPort | Translates route events to the existing typed WindPacket observation/weighted queue and verifies current revision |
| ColonyState | Adds digital_route_active metric: 1 for committed/advanced, 0 for reverted |

## Commit and recovery flow

1. Digital route World and pending receipt commit together in the SW5 ledger.
2. RouteReceiptDeliveryService encodes the pending event as a distinct typed WindPacket change.
3. WindStore writes the immutable packet object and commits its event HEAD.
4. The runtime ingests an evidence-lane WindPacket observation and the ColonyState projector materializes it.
5. The route sink returns success only after the expected value and sequence can be read coherently; the source ledger records its ACK.
6. After restart, WindStore replays verified committed frames into a fresh projector. An unfinished source ACK can then be repeated without duplicating WindStore event IDs or projector revisions.

This is a **local durable-journal + local materialization ACK**. It does not mean a remote UI, secure mesh peer, or physical device acknowledged anything. The two independent ledger HEAD commits are not one filesystem transaction; immutable identity and retry bridge their crash gap.

WindStore provides separate durable consumer cursors for subscribed observers. The tests cover subscribe, acknowledge and resume without conflating a viewer cursor with the source outbox ACK.

## Limits

- One synthetic source slot and one session namespace per active stream, configured locally. Neither is authenticated peer identity; source epochs are a future contract.
- The SW4 weighted priority queue stays intact, including its 8:5:3:2 nonfault lane weights. Route receipts use the evidence lane.
- WindStore is still bounded (4096 events, 4 MiB journal). File fsync is not appropriate on IRQ or tight sensor acquisition paths; a future Windows/Linux service worker must isolate this I/O.
- Rebuilding this stream does not reconstruct every unrelated ColonyState metric or all SIM3/SW5 histories.
- Digital-only simulated route changes. No living-cell care, feeding, stimulation, biological training or instrument actuation.

## Commands

    anaiero optimize fork demo examples/cells/crest-blood-neural.cevatlas ./route
    anaiero optimize fork sync examples/cells/crest-blood-neural.cevatlas ./route ./windstore crest-session 31

Running sync again with the same session and source slot reconstructs the latest digital route status without a duplicate event.

## Acceptance

Windows/Linux C++20 tests cover packet identity, source mismatch, malformed event kind, conflicting session+sequence, simulated crash after projector publication but before source ACK, idempotent retry, rollback preserving the culture tick, subscribed WindStore event receipts/cursors, and restart projection replay.

The next generalization is explicit source epochs and cross-engine projector recovery, followed by actual native authenticated mesh subscriptions.
