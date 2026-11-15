---
title: "CE-SW-04 — Twelve-Axis Profiles, Crystal Research Sessions and Multimodal Teaching"
project: "ChristEye Anaiero"
version: "0.1.0"
date: "2026-11-15"
status: "Proposed software/control-plane design; no physical biological actuation"
related: "CE-SW-01, CE-SW-02, CE-SW-03, BIO-04, BIO-06, BIO-09, CE-A22, P02B-02"
---

# CE-SW-04 — Twelve-Axis Control Surface, Crystal Evidence and Sensory Mesh

## 1. Scope and non-negotiable boundary

Provide twelve continuous, individually configurable **software preference controls** per colony/host/client; one reusable education strategy interface; recorded material-presence and Crystal-study evidence; realtime authorized human audio/text/video channels; and board capability advertisements for camera and audio capture.

All twelve values affect only *digital* resource allocation, curriculum/observer selection, research guidance and user-facing output. None is a direct nutrient, molecule, tissue-growth, electrode-driving, cell-joining or viral-experiment command. The independent A22 controller and qualified instrument systems are not adjustable through this surface. "Crystal consumption", biological encoding of words by peptides, induced neuronal "yearning", and knowledge transfer by material are research hypotheses, not accomplished mechanisms.

## 2. Exactly twelve axes

Store each as unsigned integer in [0, 1000] (0–100.0% with 0.1% steps); do not treat any axis as calibrated biology. Render as 0–100 in UI. Store `axis_definition_revision` and `axis_semantics_digest` with the profile. These controls remain orthogonal in the *data model*, even where resource-budget constraints couple effective values.

| ID | Name | Allowed effect of higher value | Explicit exclusion |
|---|---|---|---|
| 01 `food_support` | Food/Milk | More monitoring, support-status synthesis, team-review detail | Cannot change nutrient supply or medium |
| 02 `knowledge` | Knowledge | Greater digital Bread prerequisite/retrieval workload | Not a molecular information injection |
| 03 `growth` | Growth | More digital capacity planning and neurite/lineage observation analysis | Cannot induce, clone or direct tissue growth |
| 04 `understanding` | Understanding | More independent held-out learning and Root verification work | Not an asserted measure of consciousness |
| 05 `energy` | Energy budget | Larger *permitted* CPU/GPU allocation under device cap | Never overrides cooling/power or approved life-support |
| 06 `retention` | Retention | Stronger digital retention tests, replay/archival priority | Does not promise biological memory maintenance |
| 07 `expressiveness` | Expressiveness | Verbosity, speech detail and communication frequency | Cannot bypass consent or message budget |
| 08 `creativity` | Creativity | Higher candidate diversity in synthetic/digital generated responses | Cannot override validation or sealed evaluator |
| 09 `crystal_focus` | Crystal focus | Allocate analysis/archive budget to digital CrystalPacks and separately labelled physical-candidate study evidence | No material manufacturing or consumption control |
| 10 `teacher_balance` | Teacher balance | 0 = teacher-heavy digital curriculum, 1000 = digital CrystalPack-heavy; hybrid between | Does not mix physical intervention streams |
| 11 `environment_adaptation` | Environmental response | Shorter model *review* cadence after **valid** environment changes | No autonomous physical setpoint change |
| 12 `evidence_rigor` | Evidence rigor | Additional provenance analysis, replay comparisons, independent-review requests | Cannot relax mandatory trust/safety rules when low |

Thresholds, training split integrity and safety validity are **hard constraints**, never knob-tunable. Better "Creativity" may make more trial tasks but not more credible results. "Food" and "Growth" preferences produce advisory studies, not recipes.

## 3. Physical control surface and interface

**Rev-A prototype:** twelve endless rotary encoders with detents (interrupt/event-driven), each with push-to-select, LED ring or twelve-bar display, labels on a small OLED, one separate guarded PROFILE/LOCAL/HOST selector and one HOLD/REVERT action. Absolute potentiometers are optional with a suitable qualified ADC/multiplexer; hardware-timed ADC conversions + DMA/interrupt report changed values. Sensor sampling is not application-level message polling. Never attach encoder GPIO directly to a biological actuation signal.

For each encoder event: hardware timestamp -> debounced bounded delta -> profile edit candidate -> authorization and bounds -> versioned committed profile -> `CONTROL_PROFILE_APPLIED` receipt -> pushed effective profile. Report firmware ID, calibration/step size and local knob channel. Configuration should be accessible over authenticated WindBus without requiring a control panel.

A physical absolute potentiometer must support **soft pickup/takeover** after profile switch or remote update: values are not applied until the physical setting crosses the effective value within a deadband. Prevent unexpected jumps after reconnection. Use hardware limits and a digital reset/undo history. All input edges and revisions are auditable.

## 4. Host/client profiles, tenancy and overrides

Profile layers: `fleet_defaults -> host_budget -> colony_profile -> client_profile -> temporary_teacher_session`. A host caps CPU/GPU/memory and media delivery independently of a client's preferences. Clients may adjust axes within approved role and scope, never acquire missing capabilities.

Use an effective-policy reducer with revisioned inputs:

```text
requested(axis) = clamp(personal_profile(axis) + bounded_environment_advisory(axis), 0, 1000)
effective(axis) = constrain(requested(axis), host_budget, device_capability, role_policy)
execution_plan = admit(effective, actual_compute_capacity, queue_credits, hard_safety_limits)
```

Environmental adjustments apply only to ***digital*** observation/planning costs and must cite a current validated sensor/quality record. Missing or degraded sources produce abstention. Resource budgeting can use normalized weights `w_i=1+effective_i/1000` with a separate reserved fault lane, per-route ceilings, maximum memory and deadline admission. An axis is a preference, never guaranteed fulfilled throughput.

Profiles are atomically published with `profile_id`, `revision`, `base_revision`, `origin`, `principal`, `scope`, `signature_ref`, timestamps, affected client IDs, 12 values, applicable policy digest and immutable audit evidence. Reject conflicting base revisions with an explicit conflict receipt, rather than last-writer-wins. A 100-ms sample debounce is NOT a guarantee of biological realtime; choose measured hardware-specific bounds. Safety controllers are out of band.

## 5. Learning strategies are swappable software adapters

```cpp
enum class StudyStrategy { observe_only, teacher_led, digital_crystal_led, hybrid };
struct StudyRequest { EvidenceRefs context; CapabilityGoal goal; DigitalBudget budget; };
struct StudyOutcome { ArtifactRefs outputs; EvidenceRefs assessments; Uncertainty uncertainty; };
struct IStudyStrategy {
    virtual StudyOutcome plan(const StudyRequest&) = 0;
    virtual ~IStudyStrategy() = default;
};
```

- `TeacherLed`: human conversation, structured lessons, digitized exercises, qualified task-readout and independent assessment.
- `DigitalCrystalLed`: compatible, signed **digital** CrystalPack installation, teaching/tasks/replay followed by frozen independent evaluation.
- `Hybrid`: curriculum planner weights qualified *digital* work queues by `teacher_balance`; compares methods on independent tests.
- `ObserveOnly`: collect evidence, make nonphysical predictions and wait for authorized review.

Switch only at a recorded safe software transaction boundary: `QUIESCE -> SNAPSHOT -> DRAIN/EXPIRE -> VERIFY -> SELECT -> RESUME -> RETEST`. Preserve older Root/capability baselines. Do **not** say the two methods are biologically interchangeable or dynamically swap a live unqualified stimulation process. Any future lab-qualified interface remains separately governed.

## 6. CrystalBus research session lifecycle

Create `WindBus/Crystal` as a logically distinct, ACL-scoped set of message families on the common authenticated WindPacket mesh. A session observes the **presence and scientific evidence** surrounding a candidate material without assuming its instructional meaning.

1. `CRYSTAL_CANDIDATE_REGISTERED`: material/sample ID, custody, type, packaging/operator records, rights and preparation compatibility.
2. `PRESENCE_CANDIDATE`: a barcode/RFID scan, imaging fiducial, instrument reading or operator declaration; keep evidence method and detection uncertainty.
3. `PRESENCE_VERIFIED`: independent confirmation of time/place/identity by a qualified observer (not inferred from packet arrival alone).
4. `BASELINE_BOUND`: predeclared time windows and sensor sources, measured starting connectivity/performance and relevant approved comparator.
5. `SESSION_OBSERVATION`: immutable refs to changes in imaging tracks, RNA assay/localization observations, electrophysiology, Soil morphology, digital curriculum outcomes, missingness and potential instrumentation artifacts.
6. `MATERIAL_EXIT_RECORDED`: qualified departure event and observed interval; unknown time/identity remains unresolved.
7. `EVALUATION_COMPLETED`: independent test reports on effect, controls, retained function and alternative explanations; may be negative/unresolved.
8. `ARCHIVE_REQUESTED`: produce one content-addressed research dossier with raw-data refs, calibration/clock/custody lineage, named physical-material claim, errors and evaluation; approval-gated digital artifact promotion is separate.

Presence detection does not prove uptake, neuronal contact, molecular movement, learning or transfer. A physical candidate can be *archived as research evidence* without qualifying as an installable CrystalPack. Actual molecular material transfer or experimental control is **outside** this software specification.

## 7. Crystal session timeline and analysis inputs

Timebase records `t_pre`, `t_presence`, `t_exit`, `t_post`, each with distinct instrument clock domain, uncertainty and independent source links. Track:

- **Neurite/axon/spine**: time-aligned morphology and candidate-contact graphs, image-tracker confidence, registration error and withheld functional tests.
- **RNA-related information**: direct measured probes or assay snapshots labelled by measurement method, sample context, timing and calibration; local movement inference must be explicitly `INFERRED` and must not pretend RNA sequencing was a continuous sensor.
- **Protein / molecular observations**: measured assay results, gene/protein/pathway hypotheses, units, source and dilution/measurement uncertainty without sequences or delivery formulas.
- **Soil**: material/tissue geometry and environment observations, focus changes, drift, artifact exclusions and preparation identity.
- **Functional relation**: software/BioBridge task metrics and independently verified Root retest, not firing rate alone.
- **System health**: OEM support telemetry, faults, capture loss, drift, postprocessing versions.

A before/after difference `delta = post - baseline` is an **association**, not evidence of causality. For any causal claim require preregistered comparisons, source compatibility, independent evaluators and explicit missingness/error/confounder analysis. The CrystalFocus knob changes analytic attention and archival priority only, never validation criteria, approval, presence judgement or experimental exposure.

## 8. Realtime teacher channels

A teacher should connect directly to an enrolled client/colony in an explicit `TeacherSession`: authenticate/authorize -> select client -> negotiate text/audio/camera/video capabilities -> attach agreed task/codebook -> stream input and observe response -> record measured receipts -> independently evaluate -> close.

`TEACHER_INPUT_TEXT`, `TEACHER_INPUT_AUDIO_CHUNK`, `TEACHER_TRANSCRIPT`, `TEACHER_VISUAL_FRAME_REF`, `TEACHER_CUE_REQUEST`, `RESPONSE_TEXT`, `RESPONSE_AUDIO_CHUNK`, `TASK_RESPONSE_OBSERVED`, `TEACHER_SESSION_RECEIPT`. Record stream sequence, capture time/clock, latency, consent/scope, source hash, codec/decoder/model revision and delivery outcomes.

Human speech can directly reach a *digital* participant through ASR/NLU/text/audio processing and be answered through text or TTS. A neural preparation does **not** automatically hear or understand spoken words merely because audio data reaches its host. A biological codebook/readout requires independently qualified instrumentation and evidence of discrimination. Unreviewed speech is not an actuator command.

Avoid echoing evaluator answers through the teacher channel during blinded trials. A teacher can change digital curriculum policy only through an enrolled role, signed profile revision and explicit per-session scope.

## 9. Vision routing through the support motherboard

**Yes, add vision.** Preferred topology: USB3/UVC or USB3 Vision camera -> supported host/gateway (A-B0/A-B5) with hardware timestamps and DMA buffers -> bounded frame ring -> optional GPU inference/ROI extractor -> `WindBus/Vision` frame descriptors and interpreted observations -> authorized client/subscriber. A GigE Vision camera may directly join a qualified acquisition VLAN; use policy isolation because GigE Vision discovery/control alone does not imply secure authentication.

A resource-constrained A-B1 MCU receives timestamped ROI/feature summaries, small thumbnails or immutable evidence references, not continuous raw 4K video. Use native frame buffers on the acquisition host and a zero-copy-capable pipeline where supported. For a remote interactive teacher, deliver encoded video over an authenticated low-latency stream with separate bandwidth quotas; persist content-addressed captures only when consent and study rights allow.

`VisionFrame` should include camera_id, stream/session_id, frame_seq, acquired_time and clock_domain, exposure/trigger ID, calibration hash, width/height/pixel format/codec, frame payload or object digest, region-of-interest, missing-frame count, stage transform, privacy/rights class, analysis model digest and measured latency. Native video or an embedding is not equivalent to biological sight. Evidence that a living preparation responded to a visual task requires a separately qualified sensory encoding and frozen-reader experiment.

Avoid forcing raw imaging through 1GbE along with raw MEA data if the aggregate exceeds measured capacity. Use USB3 local capture, reserved Ethernet lanes or 10GbE/fiber where throughput demands; do not use HDMI as generic peer communication. An HDMI-output camera may be captured by a separate vendor-compatible capture adapter.

## 10. Hardware and software API

Board capability manifest adds `capture` roles `audio_input`, `audio_output`, `video_input`, `time_trigger` and independently permissioned subscriptions. Proposed supported physical topology: 1GbE Cat5e host/control link; USB3/GigE imaging; USB Audio Class or existing PC audio interface for teacher I/O; serial/fieldbus for read-only diagnostics; optional SFP+ fiber uplink.

```cpp
// API contract sketches. Not released device drivers.
ControlReceipt submit_control_profile(ProfileRevision candidate, Principal signer);
SnapshotRef get_colony_state(ColonyId colony);
Subscription subscribe_controls(ClientId client, Revision from, ControlDeltaHandler handler);
StudyOutcome select_digital_strategy(ClientId client, StudyStrategy desired, EvaluationGate gate);
CrystalSessionId register_crystal_candidate(MaterialEvidenceRef candidate);
Subscription subscribe_crystal_evidence(CrystalSessionId session, EvidenceHandler handler);
TeacherSessionId open_teacher_session(ClientId id, AuthorizedModalities modalities);
Subscription subscribe_vision(ClientId id, VideoFilter filter, FrameDescriptorHandler handler);
```

A capability can be listed but remains unusable until device enrollment, role-based ACL, resource admission and applicable study rights permit it. No analog control is ever a new physical authorization.

## 11. Acceptance tests and failure policy

1. Exactly twelve known axes and a matching definition revision; reject unknown/missing values, NaN in diagnostic exports or unsupported profiles.
2. Encoders coalesce rapid updates and never violate bounded event-ring memory; absolute knobs use soft takeover after a remote update.
3. Host caps and A22 restrictions are never relaxed by client edits or a compromised peer.
4. Missing sensor sources are marked stale; environment response cannot infer healthy biology from digital liveness.
5. Mode switch preserves prior evidence and old-skill baselines; no interrupted physical action replay.
6. Crystal presence is never inferred from network connectivity alone; custody and observation provenance are independently recorded.
7. No RNA sequencing result is misrepresented as realtime RNA tracking; morphology changes do not imply memory transfer.
8. Crystal archive produces **research dossier** unless independent installable digital CrystalPack criteria have been met.
9. Camera unplug triggers capture fault, missing-frame receipt and explicit staleness; remote teaching remains independently reconnectable.
10. Vision privacy ACL prevents unapproved live display or media replay.
11. Teacher transcript and speaker output preserve distinct provenance from any actual biological response.
12. No knob value or voice command can change food composition, feed timing, tissue growth, chemical treatment or electrophysiological intervention.

## 12. Implementation and review roadmap

- **CS0:** schemas and JSON diagnostic examples for profile, Crystal evidence session, teacher/vision session; synthetic UI preview and deterministic reducer tests.
- **CS1:** event-driven MCU encoder/ADC control driver with authentication, local/host profile reconciliation, bounded config journal and secure rollback.
- **CS2:** digital-only study strategy adapters and data-validated Bread/Meat handoff, signed controls and independent evaluator tests.
- **CS3:** synthetic Crystal presence source, evidence-timeline linkage, safe research-dossier compiler and missingness/negative-control tests.
- **CS4:** supported USB3/UVC/GigE camera adapter plus audio input/output, bounded stream publication, authorization and timestamp benchmarks.
- **CS5:** read-only qualified lab observation under the existing CE-A22/OEM safety review; no automatic biological interventions.

## References

- [CE-SW-03 colony state and support backplane](CE-SW-03_colony_intelligence_bread_meat_backplane.md)
- [CE-SW-02 developmental evidence](CE-SW-02_developmental_windbus_architecture.md)
- [BIO-04 SustiBus, ChemiBus and provisions](../../processes/BIO-04-sustibus-chemibus.md)
- [P02B-02 board topology](../../phases/P02B/docs/design/P02B-02_Board_Topology_and_Instrument_Interfaces_Design.md)
- [USB-IF USB 3.2 interface family](https://www.usb.org/usb-32-0)
- [A3 GigE Vision standard](https://www.automate.org/vision/vision-standards/vision-standards-gige-vision)
- [Linux UVC driver](https://www.kernel.org/doc/html/latest/userspace-api/media/drivers/uvcvideo.html)
