# Anaiero native WindBus SW1 prototype

**Status: digital-only portable C++20 reference implementation; not a security/biological-control release.**

This directory implements the initial **SW1** scope from [CE-SW-01](../docs/software/CE-SW-01_native_mesh_architecture.md):

- Header-only, allocation-free **WindPacket/1** encoder and decoder. Exact 56-byte little-endian header; no native-struct casting. Bounds, schema family kind, delivery class, flags, versions and TLV framing are validated. Required unknown TLVs and duplicate singleton TLVs are rejected. Payload remains opaque.
- Bounded, single-owner **weighted endpoint scheduler**, with five physically separated capacity pools, admission status, expiry receipts to the caller and wake-on-empty-to-nonempty callback. Weights 8:6:4:3:1 model *processing share*, never scientific confidence or an actuation permit.
- Deterministic tests for wire offsets, round-trips, invalid/truncated packets, unknown required TLVs, duplicate TLVs, queue overflow, expiry, fairness and randomized malformed input; an event-triggered in-process loopback demonstration.

Build (Linux/macOS/Windows with CMake 3.20+ and C++20 compiler):

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

### What this code does *not* do

- **No network transport**: the codec is not a QUIC, Noise or TLS implementation. A packet flag never authenticates a peer. Future adapters must verify the secure session, ACL, source-slot mapping, schema and consent scope before accepting records.
- **No durable delivery**: `accepted` means an in-memory queue slot, not persisted, network acknowledged, delivered or biologically acted upon. Future SW2 adds WindStore/WindChange durability.
- **No direct sensor / neural / actuator driver**: only opaque packet bytes and caller-owned work tokens. No chemical, molecular, stimulation or pathogen operations are present.
- **No multithreaded queue claim**: the scheduler is explicitly single-owner; platform producers serialize into it before calling `post`. An ISR must only signal a task / DMA descriptor and must not parse untrusted packets.
- **No background polling**: a platform callback or notified task posts work, and the recipient drains `next()` until empty. Periodic status timers are not transport delivery loops.
- **No time-domain conversion**: deadlines use the caller-provided monotonic clock domain. Clock anchors, uncertainty, persistence and provenance are future integration requirements.

### SW2 and SW3 follow-ups

Implement a native read-only ledger, durable receipt state machine, hash-addressed immutable objects, synchronized connection handshake, authenticated identity and replay protection, with CPU/MCU performance tests and formal wire compatibility vectors. The Python reference remains supported during migration. This component has no proven worst-case interrupt or network latency and has not yet been compiled for a specific MCU toolchain.

## SW1.1 hardening (this branch)

- WindPacket encoding detects input/output buffer overlap and returns `overlapping_buffers` before modifying the output frame. Its decoded `View` remains a **borrowed**, synchronous reference; callers must keep the underlying input bytes alive until consumed or copy an owned frame into their bounded storage before asynchronous dispatch.
- Fault notifications are dispatched ahead of weighted digital work. The other four queues use *smooth* weighted round robin (6:4:3:1), avoiding long consecutive bursts from grouped slot scheduling. This is **item-weighted**, not byte-cost fair scheduling; reliable byte-credit/bandwidth admission remains a future layer. Independent hardware A22 limits never depend on queue dispatch.
- Callbacks and posting remain **single-owner**: real multithreaded/ISR producers need a separately synchronized bounded ingress ring. No raw ISR directly parses untrusted frames.

## AVM1 virtual PCM and MIDI patchbay

`include/christeye/wind/media_patchbay.hpp` adds a freestanding-compatible, fixed-capacity, digital-only software routing core. Ports have stable numeric IDs, PCM/MIDI media kind, producer or consumer role, exact clock domain/sample-rate/channel matching and an upstream `verified` flag. Route commits require an authorized caller and matching expected revision. The patchbay synchronously fans out **borrowed** PCM blocks (interleaved float samples) or UMP word arrays to pre-registered callbacks. Hot-unplug invalidates affected routes and increments muted-route receipts.

It does **not** implement operating-system drivers, ringbuffer scheduling, converter/sample-rate conversion, real MIDI-CI device discovery, audio mixing, cryptographic verification, physical audio I/O, network media, neuronal speech, or biological actuation. Integrators must validate `verified` using actual signed installed-hardware manifests/ACLs; a user-provided boolean is not authentication. The callback is synchronous and must be nonblocking; the samples are invalid after the callback returns unless copied by the consumer.

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

Additional `wind_media_tests` verify two-route audio fan-out, typed MIDI delivery, mismatched clocks/rates, malformed frames, stale route revision, unauthorized route, queue capacity and removal-induced muting. No timing benchmark or device validation is claimed.

See [CE-SW-05](../docs/software/CE-SW-05_modular_audio_vision_midi_starchip.md) for the hardware-model and research-safety contract.
