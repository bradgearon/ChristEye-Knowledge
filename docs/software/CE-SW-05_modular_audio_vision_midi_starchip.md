---
title: "CE-SW-05 — Modular Audio, Vision, MIDI and Speech StarChip"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
classification: "electrical/software architecture; digital audio/MIDI prototype and read-only neural research boundary"
status: "Design proposal; no custom PCB, neural speech interface or hardware qualification completed"
related: "CE-SW-01, CE-SW-02, CE-SW-03, CE-SW-04, P02B-02, P02B-06, CE-A11, CE-A22"
---

# CE-SW-05 — Audio/Vision/MIDI StarChip and Patchbay

## 1. Purpose

Create a pluggable **Audio/Vision/MIDI StarChip (AVM)** family: multichannel high-quality microphone/line capture, configurable DAC playback, replaceable analog amplification daughtercards, MIDI 1.0/2.0 events, real-time camera/feature routes, and a teacher conversation path. A client, host or colony receives only the capabilities **physically installed, verified, provisioned, and authorized** for the particular board.

These are *electronic input/output capabilities* and a software platform for research. A hypothetical biological "wind of language" is not proven. Future evidence of a qualified living neural system producing intentional speech must come from separately approved interface experiments with independent training/holdout testing. The AVM module never directly connects to electrodes, changes a neuronal stimulation waveform, drives culture/feeding equipment, or translates a model's text into electrical stimulation.

## 2. Naming and scope

- **AVM StarChip** = a physically attachable media interface module plus host software service.
- **MediaPatchbay** = software graph connecting authorized channel endpoints; it is distinct from WindBus control/evidence planes.
- **VoiceBridge** = supervised digital conversation adapter (mic -> transcription/teacher context -> authorized digital learner -> TTS/PCM -> speaker).
- **NeuralSpeechObservation** = research-only timestamped **readout** of a vendor-qualified neural device and a frozen decoder's inferred vocal symbol/intent. A decoded output remains a *prediction*, not a proven volitional utterance.
- **Audio channels** = actual sampled PCM streams, not merely textual WindPackets.
- **MIDI channels** = MIDI messages/Universal MIDI Packets, not audio samples.
- **Video channels** = timestamped frames/encoded video or feature vectors, not electrical stimulation.

## 3. Hardware block diagram

```text
   balanced MIC / LINE --> protection --> switchable gain / replaceable AFE --> anti-alias --> ADC
                                                                                   |
   MIDI DIN / USB MIDI --> isolated / class-compliant MIDI frontend ---+           |
   USB3 camera / GigE camera --> vendor capture --> frame clock ------+|           |
   OEM MEA --> read-only vendor API --> research feature adapter -----+||          |
                                                                     vvvv          v
                         [AVM carrier: power + codec/DMA + reference clocks + secure identity]
                                             |
                  USB Audio Class 2.0 / USB3 / 1GbE or 10GbE media uplink
                                             |
                            [HOST: realtime media engine + MediaPatchbay]
                                  |                   |
                             WindBus control      media buffers
                             evidence/receipts    timestamped PCM/UMP/video
                                  |                   |
                                  +----> digital decoder/TTS/synth ----+
                                                                         |
    headphone / speaker <-- output amp <-- replaceable output stage <-- DAC <--+
```

Physical lane boundaries: analog audio / converters live in a separate low-noise dry electronics zone. Vendor MEA contact, acquisition and any independently approved physical inhibit remain on their existing vendor system. AVM's line and headphone outputs are **never** wired to electrodes or a biological preparation. A qualified lab can import *observation records* from OEM neural interfaces over its separate adapter.

## 4. Replaceable op-amp stages

Use **keyed, power-off-replaceable analog daughtercards**, rather than allowing arbitrary loose op-amps to be swapped while powered. Carrier-side standardized footprints/edge sockets can expose documented supply rails, ground/reference, differential inputs/outputs, mute, card-present ID, temperature and calibration storage. Analog revisions require electrical validation for stability, supply headroom, offset, bandwidth, load and EMC before certification.

Define distinct card roles:
1. **MicPre:** low-noise microphone input with balanced differential capture, hardware gain staging, clipping indicator and separate controlled phantom-power capability *only on a rated microphone port*.
2. **LineIn:** balanced/unbalanced line receiver, fixed/trim gain and input protection.
3. **LineOut:** DAC reconstruction filter + replaceable output buffer/driver, controlled muting and output limits.
4. **HeadphoneOut:** impedance/load-rated headphone amplifier, pop suppression and optional hardware limiter.
5. **ResearchAFEAdapter:** vendor-qualified observation adapter, never a custom direct electrode driver.

A daughtercard provides its own `module_id`, `serial`, `electrical_profile_digest`, `analog_role`, `compatible_carrier_revisions`, `verified_gain_range_db`, `supply_profile`, `calibration_digest`, `test_status` and replaceable-in-service **false**. A change of card forces channel deactivation, mute/restart, capability revision, recalibration and a new signed compatibility receipt. Never assume pin-compatible audio op-amps are functionally drop-in replacements.

## 5. Audio data path and quality contracts

Design targets (not measured guarantees):
- Default multichannel capture/playback **24-bit PCM at 48 kHz**; configurable 96 kHz and optional 192 kHz according to ADC/DAC and transport profile.
- Distinguish physical converter effective-number-of-bits/SNR/THD+N from nominal 24-bit transport resolution.
- Transport PCM as integer 24-in-32 or float32 in host buffers as negotiated; do not silently quantize without metadata.
- An `AudioPort` advertises direction, channel count, connector class, codec/ADC/DAC model ID, installed sample rates/format set, speaker/headphone/phantom/gain capabilities, measured input/output latency and calibration/clock IDs.
- The realtime engine uses preallocated buffers, no heap allocation / disk write / blocking network I/O inside the audio callback, bounded callback work, lock-free single-producer/single-consumer descriptors where appropriate and a separate control executor.
- Event-driven *audio hardware* necessarily uses a continuous sample clock and periodic DMA-buffer completion interrupts; this is **not** forbidden application-level network polling.
- Gap, underrun/overrun, late frame, clipping, muted/stale output, unsynchronized clock and sample-rate-conversion events produce counters and stamped WindBus receipts.

Nominal uncompressed bandwidth formula:
[
R_{pcm}=channels \,\times\, sample\_rate \,\times\, sample\_bytes.
]
For example, 8 channels × 96,000 samples/s × 4 bytes = **3,072,000 bytes/s per direction** before transport overhead. Camera/video throughput can be much larger and needs separate limits.

## 6. MIDI implementation

**MIDI 2.0 Universal MIDI Packet (UMP)** is the canonical internal MIDI event representation. MIDI 1.0 byte stream inputs can be translated by a versioned adapter without pretending that precision was added. Advertise **MIDI-CI** support, UMP groups, device function blocks, active profiles, property exchange support, and direction independently. Each endpoint records whether it supports MIDI 2.0 protocol, MIDI 1.0 within UMP, USB MIDI transport, DIN MIDI transport, or network UDP MIDI 2.0 transport; never assert USB-MIDI 2.0 merely because a device accepts MIDI 1.0.

Reference physical interface options: class-compliant USB MIDI; optically isolated MIDI IN DIN interface and properly rated output for physical ports; dedicated clock input if supported; optional MIDI timecode/transport metadata through the host. Musical events carry a media clock timestamp and their own ordering independent of packet-arrival time.

MIDI can drive an **authorized digital sound engine or visualizer**, and can capture gestures/control data for a teacher session. It does not grant permission to actuate instruments connected to the biological preparation.

## 7. Vision/media integration

USB3 cameras are hosted via their existing supported vendor driver/UVC APIs; supported GigE Vision cameras may reside on a dedicated acquisition network. Camera metadata/payload is published to a media pipeline with backpressure and frame-loss accounting. Downstream MCU nodes should receive approved low-bandwidth ROI, features and frame references rather than raw UHD video.

Each `VideoPort` declares physical backend, sensor/camera ID, resolution/fps pixel format and codec sets, calibration, exposure/trigger clock, buffer ceilings, privacy classification and feature-extraction support. Camera access is consent-scoped independently of audio and MIDI.

## 8. Native media channel routing: OS patchbay

A channel is a typed endpoint with **stable port ID**, not a global integer: `avm/{device_id}/{audio|midi|video}/{in|out}/{port}/{channel}`.

Routes are described declaratively and applied transactionally:
- `AUDIO_PCM` may route only to compatible PCM consumers, optional sample-rate converter, mixer or a qualified digital decoder.
- `MIDI_UMP` may route to MIDI-capable engines and only through versioned translators when required.
- `VIDEO_FRAME` may route to image analyzers, teacher display or archival capture after privacy admission.
- `NEURAL_OBSERVATION` may enter a research decoder through **read-only vendor bridge**; no direct reverse actuation path exists.

Before a route is committed: authenticate the principal, verify installed/enabled and authorized port, check type compatibility, sample/clock format, route cycle detection, bounded latency/bandwidth/CPU reservation, human consent/rights, revision and emergency mute. Admission either commits atomically or returns a rejection receipt; no half-formed routes and no cross-tenant wildcard audio subscriptions.

Examples:
- teacher mic L/R -> audio monitor + speech recognition;
- MIDI keyboard UMP group 0 -> authorized digital synth -> stereo output pair;
- approved camera -> feature decoder -> teacher digital context;
- qualified OEM neural readout -> fixed candidate-speech decoder -> evidence/log and optional *digital* synthesized speech after review.

**The realtime media ring is NOT the WindPacket control bus.** WindBus carries discovery, routing changes, timestamps, content references and evidence; media payloads use dedicated bounded DMA/USB/network streams. The host may store audio as immutable chunks under a consent policy without storing every sample in the packet log.

## 9. Audio/video network placement

- **Local host:** USB Audio Class 2.0 for straightforward multichannel USB devices, supported OS audio stack or vendor ASIO where appropriate; use the hardware's reported capabilities.
- **Motherboard Ethernet:** Cat5e/1GbE for control, MIDI, provenance, small audio channel counts and most state traffic, provided measured capacity is adequate.
- **Professional audio over IP:** optional AES67-over-IP interoperability or Milan/AVB profiles on suitable hardware and switch infrastructure, with measured clocking/jitter and application-level authentication/ACL. These standards do not themselves mean arbitrary WAN media streams are encrypted.
- **High-bandwidth media:** SFP+/10GbE or supported direct USB3 for high channel counts, multiple camera feeds and low-contention acquisition.
- **HDMI:** external video display/capture connector only when a specific module requires it, not the default peer-to-peer mesh.
- **Serial/CAN:** board status and low-rate controls, not raw high-resolution multi-channel PCM video.

For professional-grade audio, PTP/802.1AS support should be declared and measured, not merely assumed because Ethernet is present. For remote teacher sessions, a vetted interactive encrypted media protocol (for example WebRTC with approved identity and session integration) is an optional gateway, distinct from the native local recording path.

## 10. Three provisional board models, with independently installed options

These are **design tiers**, not tested manufacturing SKUs or specific IC commitments.

| Tier | Hardware example | Media capabilities | Physical profile |
|---|---|---|---|
| **AVM-V1 VoiceBridge** | 2 input / 2 output analog channels | PCM 48/96 kHz, USB Audio 2.0, stereo digital voice, basic MIDI 1-to-UMP adapter; camera metadata only | low-power dry-zone daughtercard |
| **AVM-S8 StudioMesh** | 8 input / 8 output channels | 24-bit PCM 48/96/192 kHz if converter fitted, 4 MID I UMP groups, optional DIN, gain/monitor patchbay and USB3 capture | host-connected, replaceable AFE banks |
| **AVM-R16 ResearchMedia** | Up to 16 input / 16 output channels | 48/96 kHz, 16 UMP groups if controller supports, clock/timestamp capture, 2 USB3 camera ports, optional SFP+ uplink, qualified read-only OEM research feature lane | rack/backplane with isolated instrument interfaces |

Always derive capabilities from `model catalog ∩ installed modules ∩ firmware/driver support ∩ measured constraints ∩ authorized policies`. Model tier alone does not give permission or prove hardware exists. A host can be a pure software AVM adapter with no analog I/O.

## 11. Capability advert: supported vs installed vs available vs authorized

Every device and daughtercard has a signed versioned `MediaModuleManifest`; it contains:
- model_id, serial, carrier PCB revision, firmware/software digest, secure board principal, inventory epoch and manifest revision;
- enumerated port records with stable `port_id`, media_kind, direction, connector, channel range, supported sample formats/rates, clock domain, mute/gain/monitor/phantom features and option dependencies;
- actual installed op-amp/codec/MIDI/camera modules and calibration/qualification receipts;
- required host driver/API and optional media transports; IRQ/DMA descriptor budget, local buffering and nominal/measured throughput;
- expected resource draw and power/isolation boundaries;
- explicit feature states `CATALOG_ONLY`, `INSTALLED`, `VERIFIED`, `ACTIVE`, `DEGRADED`, `REMOVED`;
- per-route authorization and actual readiness are calculated separately. A catalog-supported port without fitted electronics cannot be routed.

Manifest hash and digital signature refer to immutable data; firmware/board attestation must be verified separately. Module unplug changes the inventory epoch and emits `MEDIA_CAPABILITY_CHANGED`; affected routes are muted/invalidated and require re-admission after new calibration.

## 12. Teacher voice and possible future biological speech

Today's operable digital path is:
```text
 teacher microphone -> ADC/PCM -> teacher-session ASR/voice analyzer
    -> authorized digital learning context/codebook -> software answer/speech model
    -> TTS/PCM digital synthesis -> DAC -> line/headphone/speaker
```

For a future research question "Can a living preparation communicate a chosen word?":
```text
 qualified vendor neural readout -> timestamped observations
    -> preregistered, frozen decoder -> output class/uncertainty
    -> independent held-out test -> candidate digital vocal symbol
    -> optional explicit human-reviewed speech synthesis (not electrode output)
```

The electrical audio module is a **readout and playback platform**, not a demonstrated biological language organ. Training living tissue to interpret or produce language, selection of electrodes, stimulus parameters, delivery waveforms and reinforcement biology are all outside this AVM design and require separately qualified research. Model-generated words, a decoded spike pattern and a human-verified intentional utterance are three different evidential claims.

## 13. Bus messages and state updates

Control plane events: `MEDIA_MODULE_DETECTED`, `MEDIA_INVENTORY_VERIFIED`, `MEDIA_CAPABILITIES_REVISION`, `MEDIA_ROUTE_PROPOSED`, `MEDIA_ROUTE_APPLIED`, `MEDIA_ROUTE_REJECTED`, `MEDIA_STREAM_STARTED`, `MEDIA_XRUN`, `MEDIA_CLOCK_DRIFT`, `MEDIA_PORT_MUTED`, `MEDIA_MODULE_REMOVED`, `MIDI_CI_NEGOTIATED`, `MIDI_UMP_EVENT_REF`, `TEACHER_AUDIO_CAPTURE_REF`, `SPEECH_DECODER_RESULT`, `SPEECH_EVALUATOR_VERDICT`.

To expose always-available colony state, emit revisioned deltas including inventory digest, port/media health, synchronization, per-port xrun/loss counters, measured buffer latency, active-route digests, connected teacher session, consent, and decoder evaluation provenance. Never store sensitive unredacted audio by default; require appropriate consent, retention policy and purpose.

## 14. Clock, latency and safety requirements

Clock owners: audio codec sample counter, MIDI media-timestamp domain, camera exposure trigger, MEA vendor acquisition clock and network PTP when available. Transport arrival timestamps are not substitutes for acquisition timestamps. `ClockAnchor` records conversion offset/drift plus uncertainty; an unsynchronized pair cannot be declared sample-aligned.

Measured metrics per route: input-to-buffer, patchbay processing, buffer-to-DAC, round-trip monitor latency, p50/p95/p99, max verified, xrun count, jitter, drift and missing-frame count. Define product-specific SLOs **after real hardware tests**, not by implying a zero-latency deterministic network.

Local audio output has an independently enforced safe-mute default, output-level limit and output-path protections appropriate to its electrical role. Control-plane loss mutes or falls back to explicitly authorized local digital monitoring per profile, never routes media into life-support or biological electrical inputs. Hot-unplug of a card must not connect an unintended signal or force a default unreviewed route.

## 15. Software implementation split

```cpp
// Concept-level C++20 contracts, not compiled production interfaces.
struct MediaPortDescriptor { PortId id; MediaKind kind; Direction direction;
  FormatCaps formats; ClockId clock; ChannelCaps channels; };
struct InstalledMediaManifest { DeviceId device; ModelId model;
  Revision revision; Digest inventory_digest; Span<MediaPortDescriptor> ports; };
struct MediaRouteChange { Revision expected_route_revision;
  PortId source; PortId destination; Format selected; Budget latency_budget; };
struct IAudioFrameSink { virtual TakeStatus submit(ReadOnlyAudioBlock block) noexcept = 0; };
struct IMidiUmpSink { virtual TakeStatus submit(UmpMessage event) noexcept = 0; };
struct IVideoFrameSink { virtual TakeStatus submit(ReadOnlyFrameDescriptor frame) noexcept = 0; };
struct ICapabilityObserver { virtual void on_manifest_revision(DeviceId, Revision) = 0; };
```

Split into freestanding MCU descriptor/diagnostic layer, host media capture/renderer, routable low-latency patchbay, authorized WindBus API, and read-only scientific evidence adapter. Driver DSP paths do not depend on heap allocation, blocking consensus, Git commits, ML inference or network round trips to deliver a local output buffer.

## 16. Verification matrix and implementation milestones

**AVM0 — Schema and simulation:** manifest schema + route schema + three example model configurations; pass positive/negative contract tests; unavailable/removed port routes fail closed; client cannot mint a hardware capability.

**AVM1 — Software patchbay:** virtual PCM + MIDI UMP sources/sinks, deterministic timestamps, channel-count/format negotiation, bounded ringbuffers, zero-copy ownership where possible, route commits/rollback and forced-mute tests.

**AVM2 — Vendor hardware loopback:** supported USB audio device, USB MIDI and camera. Measure sample-loss, latency, channel separation, clock drift, MIDI timing and hot unplug. No custom PCB required.

**AVM3 — Replaceable AFE test carrier:** dry-zone inert audio test loads; electrical calibration, gain/noise/crosstalk, stability, EMI, mute/pop suppression, power-off removal and board identification. Only hardware-qualified variants are enabled.

**AVM4 — High-bandwidth network:** optional interoperable AES67/Milan profile, tested switch/PTP configuration, bandwidth admission, secure session control and explicit loss recovery.

**AVM5 — Read-only experimental neural observation:** only with separate instrument vendor approval, investigator oversight and independent acquisition validity; speech-decoder performance evaluated against baselines and held-out tasks. Does not establish direct neuronal language competence.

### Mandatory negative tests

1. Wrong analog card revision / missing calibration / hot-unplug -> port deactivated and affected routes muted.
2. A catalog-only model feature cannot be enabled without installed and verified hardware.
3. MIDI 1.0 input never advertised as 32-bit-resolution MIDI 2.0 by translation alone.
4. Routing audio to an MEA instrument or living preparation is categorically forbidden.
5. An untrusted teacher cannot subscribe to microphones/cameras or read protected recordings.
6. Missing PTP/clock anchors prevent claims of cross-device phase/sample synchronization.
7. An audio callback does not invoke storage, network I/O, blocking locks or arbitrary model inference.
8. An AVM disconnect cannot change A22 independent physical interlock behavior.
9. Neural spike patterns without preregistered decoder and held-out testing are not labeled intentional speech.
10. Physical software knob cannot authorize unlisted I/O, phantom power, or an electrical output route.

## 17. References and status

- [CE-SW-04 teaching/vision profile](CE-SW-04_control_surface_crystal_vision_teaching.md)
- [CE-SW-03 colony backplane](CE-SW-03_colony_intelligence_bread_meat_backplane.md)
- [P02B-02 board topology](../../phases/P02B/docs/design/P02B-02_Board_Topology_and_Instrument_Interfaces_Design.md)
- [P02B-09 neural acquisition/timebase](../../phases/P02B/docs/design/P02B-09_Neural_Acquisition_and_Multimodal_Time_Base_Design.md)
- [MIDI 2.0 core specification collection](https://midi.org/midi-2-0-core-specification-collection)
- [MIDI 2.0 kernel UMP model](https://www.kernel.org/doc/html/latest/sound/designs/midi-2.0.html)
- [AES67-2023 standard](https://www.aes.org/publications/standards/preview.cfm?ID=96)
- [Milan AVB interoperability specification](https://avnu.org/resource/milan-specification/)
- [IEEE 802.1AS clock synchronization](https://www.ieee802.org/1/pages/802.1as.html)

**Status:** architecture and compatibility contracts proposed. No real circuit, op-amp/AFE board, 192-kHz converter, multi-channel codec, authenticated media stream or neuronal language interface has been manufactured or validated.
