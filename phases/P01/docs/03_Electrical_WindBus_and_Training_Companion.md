---
title: "Electrical Windbus And Training Companion"
document_id: "CE-AN-P01-03"
program: "ChristEye Anaiero"
phase: "P01"
revision: "4.0 / 2026-11-15"
status: "Engineering research specification — not a validated biological protocol"
owner: "We Develop Software LLC"
---

# Electrical Windbus And Training Companion

> **ChristEye Anaiero · Phase P01 · 15 November 2026 · WDS research property**  
> [Phase index](README.md) · [Bibliography](../references.md) · [Registers](../registers/README.md)

*Hardware boundaries, digital message contracts, calibration, closed-loop stages and module joining.*

Status: scoped engineering design and supplier-referenced research. Values tied to one platform are not transferable to another without compatibility qualification. Current tissue work: NOT EXECUTED. \[R01, R13-R16\]

# Separate the two reference instrument configurations

| **Feature**        | **Axion 24-well reference**                                                | **MaxOne+ high density research candidate**                                     |
|--------------------|----------------------------------------------------------------------------|---------------------------------------------------------------------------------|
| Biological counts  | 50k-100k live neurons/well plus astrocytes; 80k+20k worked example [R01](../references.md#r01) | NO COUNT QUALIFIED: different plate/active geometry; cannot borrow Axion count. |
| Electrode platform | CytoView 24-well MEA and Maestro Edge in source protocol [R01](../references.md#r01)           | 26,400 sites switched to 1,020 readout channels [R03](../references.md#r03)                         |
| Sampling           | Use the model actually selected and validated                              | 20 kHz per channel [R03](../references.md#r03)                                                      |
| Active dimensions  | 24-well vendor geometry, not MaxOne                                        | 3.85 x 2.10 mm = 8.085 mm2; ring inner diameter 19 mm [R03](../references.md#r03)                   |
| Data bandwidth     | Determine per Axion SDK and stored sample format                           | 1,020 x 20,000 x 2 byte=40.8 MB/s assumed 16-bit raw sample, before overhead    |
| Stimulation        | Qualified vendor profiles only                                             | Vendor API supports stimulus interfaces; limits unspecified until qualified     |

# Distinct runtime responsibilities

| **Software/service**     | **Owns**                                                                                          | **Cannot own**                                                     |
|--------------------------|---------------------------------------------------------------------------------------------------|--------------------------------------------------------------------|
| H host / WindBus         | Clock, source credentials, routing, input encoding, acquisition records, frozen decoder and queue | Final independent protected test labels or override L.             |
| Lotus A12                | Curriculum actions, available routing, task deferral and explanation of uncertain state           | Unreviewed material or stimulation parameters.                     |
| E evaluator              | Training target oracle after trial closure; sealed confirmation labels and analyses               | Live cue encoding or adaptive labels during evaluation.            |
| L independent limits A22 | Permit for qualified profiles, fault state, stale sensor and verified suppression                 | Learning goals or biologically inferred consent.                   |
| C/O/S/B                  | Qualified response class, measured timestamps and assigned experiments                            | Self-rewriting decoder, safety limits or independent data leakage. |

# Reference software task language (no live waveform values)

| **Context**     | **Sequence** | **Expected protected output** |
|-----------------|--------------|-------------------------------|
| C0              | A then B     | X                             |
| C0              | B then A     | Y                             |
| C1              | A then B     | Y                             |
| C1              | B then A     | X                             |
| C0 or C1        | AA or BB     | HOLD                          |
| Missing/invalid | Any          | UNRESOLVED host status        |

The context/order rule is inherited from Physical Device Design v0.4 and Research and System Design. There are eight valid context+sequence combinations, with HOLD present in four; use macro recall (X/Y/HOLD), not raw accuracy alone. HOLD due to valid task response is distinct from invalid observation or UNRESOLVED software state. \[R13,R14\]

# Prototype typed WindEvent

> {  
> "event_id":"uuid", "trial_id":"uuid", "source":"H:encoder",  
> "recipient":"C:input", "codebook_id":"c0ab-v1",  
> "symbol":"A", "semantic_role":"teaching_cue",  
> "planned_time_ns":123, "delivered_time_ns":null,  
> "profile_id":"QUALIFIED_PROFILE_REQUIRED", "validity":"PENDING",  
> "information_class":"cue", "stream_hash":"sha256:...",  
> "data_split":"development", "physical_authorization":false  
> }

# Closed-loop training as a state machine

1.  READY: verify source identity, instrument clock, chemistry/health freshness, approved profile IDs, stored configuration digest and available capacity; L must permit the run.

2.  ENCODE: compile a versioned context/sequence into laboratory-approved stimulus profile IDs, never an improvised pulse amplitude or charge.

3.  DELIVER: record the ACTUAL electrode mapping and timing, artifact exclusion and any underruns; rejected events are not silently retried.

4.  READ: acquire raw traces and validity masks. Decoder is trained on a separate calibration split and remains frozen during learning confirmation.

5.  SCORE: close the response window, then allow E to compute correctness for training only; invalid observations never receive fabricated success/failure.

6.  FEEDBACK: deliver one of the qualified information-feedback classes when permitted; basic medium/care never changes due to correct/incorrect answer.

7.  INTERVENE: record planned rest/replay/quiet observational period as a condition; rest is not assumed to equal sleep or memory consolidation.

8.  PROBE: evaluate the declared delayed horizon under frozen reader and no answer-bearing feedback; score separately from training.

9.  PRESERVE: write complete event lineage, reader digest, Root outcome and correction/retraining decision.

# Calibration before teaching and before any module bridge

| **Calibration** | **Measurement**                                                              | **Why it blocks an experiment**                                            |
|-----------------|------------------------------------------------------------------------------|----------------------------------------------------------------------------|
| Clock           | Requested/delivered/recorded time, drift, jitter, sync offset                | Without ordering, a Prosody claim is invalid.                              |
| Input validity  | Electrode profile ID, measured delivery and route status                     | If cue was absent, a neural learning failure cannot be inferred.           |
| Noise           | Baseline, artifact masks, usable channels and waveform quality               | Spike count/response class may be invalid.                                 |
| Response map    | Distinguishable inputs, output separability and repeated valid responses     | No trained class identity without controllable I/O.                        |
| Decoder         | Feature list, normalization, calibration set, checksum and frozen output     | Changing software can masquerade as changed tissue.                        |
| Bridge          | A/B own decoders, bandwidth, receiver codebook, missed events and disconnect | Shared output route is not an independently learned B capability.          |
| Watchdog        | Host loss, stale commands and permit shutdown under a cell-free test load    | Must fail closed for stimulus while qualified support remains independent. |

# The connective-controller layering

> APP / DATA SOURCES -\> A19 router -\> A11 WindBus -\> H input codebook  
> \| \|  
> L independent permit -\> C living interface  
> C measured signals -\> H decoder -\> O observer -\> A12 Lotus advice -\> L  
> -\> E protected scoring -\> A16 Roots / A02 archive  
> B recipient joins ONLY via versioned H bridge; caches cleared on B-alone tests.

# Progressive joining (J0–J6)

| **Stage**      | **What changes**                                            | **Exit test**                                                                 |
|----------------|-------------------------------------------------------------|-------------------------------------------------------------------------------|
| J0 independent | C and O/B independently qualified in their own compartments | Individual readiness/identity/support and frozen readout.                     |
| J1 shadow      | C state forwarded to O; O cannot act on C                   | Real-time vs offline prediction; no target leakage.                           |
| J2 advisory    | O status may propose one bounded review/routing action      | Action-matched digital baseline and L veto pass.                              |
| J3 coupled     | Fixed electronic bridge used during known trials            | C+O vs C alone / untrained O / shuffled or replay.                            |
| J4 support     | S learns to propose limited classes, in shadow first        | Benefit beyond fixed schedule and protected Root retention.                   |
| J5 physical    | New preparation version with candidate axonal connectivity  | Physical route attribution; this is not software plug-and-play [R11](../references.md#r11).       |
| J6 expansion   | New independently qualified module or replacement           | New benefit, old-task noninferiority, resource budget and immutable identity. |

# No invented stimulus recipe

Electrical charge is Q=integral I(t)dt; electrical energy is E=integral V(t)I(t)dt. Current squared integrated alone is not a general electrical-energy measurement without impedance assumptions. Qualified vendor envelope, electrode permissible charge, train frequency/timing and tissue-specific exposure limits are controlled parameters in L and must not be derived from a metaphor. \[R03, R13, R15\]

# Claim-specific acceptance

- G1 source identity, G2 maturation and health, G3 valid electrical I/O precede any claim of learning.

- Immediate learning requires comparison with no-training and noncontingent exposure controls; fixed reader, locked data splits and independent preparations.

- Delayed Roots require absolute task performance at named delay, with no answer-bearing feedback and complete validity/uncertainty.

- Joined modules must preserve C performance and add marginal value beyond equal-information software and replay controls.

- A recipient learns independently only when B-alone tests pass after verified removal of teacher state and caches.

# Selected source register

[R01](../references.md#r01) BrainXell/Axion, Neuron/Astrocyte 24-well MEA Co-culture Protocol v10.0. Published platform-specific 50k-100k neurons/well; 2:1-6:1 neuron/astrocyte ratios; 80k+20k worked example; supported medium/service schedule. https://www.axionbiosystems.com/sites/default/files/resources/BrainXell%2024-well%20MEA%20Co-culture%20%20Neuron_Astrocyte%20Protocol%20v10.pdf

[R03](../references.md#r03) MaxWell Biosystems, MaxOne single-well HD-MEA specifications. 26,400 electrode sites, 1,020 simultaneous readouts, 20 kHz/channel, 19-mm inner ring, 3.85x2.10-mm active area, API and raw data. https://www.mxwbio.com/products/maxone

[R06](../references.md#r06) Robbins et al., Goal-directed learning in cortical organoids, Cell Reports 2026. Adaptive feedback learning on task, but no retained improvement after tested 45-minute rest. https://pubmed.ncbi.nlm.nih.gov/41720084/

[R07](../references.md#r07) Cai et al., Brain organoid reservoir computing for artificial intelligence, Nature Electronics 2023. A bounded hybrid neural-computing precedent, not a full semantic mind. https://www.nature.com/articles/s41928-023-01069-w

[R11](../references.md#r11) Osaki et al., Reciprocally connected cerebral organoids, Nature Communications 2024. Axonal joining precedent, not memory transfer. https://doi.org/10.1038/s41467-024-46787-7

[R13](../references.md#r13) Brad Gearon, ChristEye Physical Learning Device Design v0.4 (provided). Current reference device; C, O, S, B, H, L, E and R0-R3 sequence. research/source_documents/R04_ChristEye_Physical_Learning_Device_Design_v0_4_3\_.docx

[R14](../references.md#r14) ChristEye, Research and System Design, revised synthesis (provided). Current device and Crystal research boundaries; experiments EX01-EX12. research/source_documents/R02_02_Research_and_System_Design_1\_.docx

[R15](../references.md#r15) ChristEye, Formal Models and Reconciliation (provided). Corrected equations, accounting, identifiability and unsupported analogies. research/source_documents/R03_03_Formal_Models_and_Reconciliation_1\_.docx

All primary source identifiers and the original documents are also preserved in the repository research directory. Earlier source syntheses do not substitute for biological confirmation.

