---
title: "S01 StarChip Studio and Demonstration Strategy"
date: 2026-11-15
status: "Specification and output-gate design"
---

# Show the work, not a misleading biological animation

The StarChip Studio should be a place where a visitor can **choose a chip, see its inputs and output, run a real bounded example when code exists, inspect the proof, and follow the system wiring**. It must be enjoyable for a game/music creator without using synthetic neuroscience as real evidence.

[Full demo registry](demo-catalog.json) specifies the current source, implementation status, input/output contract, proof requirement and proposed delivery stage for each demo.

## Distinguish real, simulated and proposed immediately

| Presentation | User sees | Evidence |
|---|---|---|
| **Working digital example** | a deterministic run or actual recorded result from the Python reference | repository source, test output, input fixture, runtime version |
| **Browser simulation** | front-end model with labeled synthetic data and known limitations | deterministic seed, fixture/schema, code link |
| **Architectural preview** | storyboard of expected input/output, without a play button | link to manifest, design, acceptance criteria |
| **Biological research illustration** | candidate tissue and measurement diagrams with interpretation status | source paper/citation and explicit absence of local validated dataset |

Do not display a mock neural-growth chart with "100% trained mind" or a neural chip responding to real user commands unless there is a real, reviewed dataset and a permitted connection.

## Stage A — four grounded digital labs

**L01 Digital Crystal replay:** User selects a fictional experience about a book or Metroidvania room. The demo records consent scope and event ID, packages immutable data, clones the pack by hash, replays it into a separate MemoryBank and shows a retrieval result. Beside the output, show the distinction between **byte clone**, **retrieval**, **learning after experience replay** and **physical memory transfer (unproven)**. Actual source: Anaiero's Crystal and memory modules.

**L02 WindBus route lab:** The visitor inspects a received task, an authorized observation, a duplicate, expired message and forbidden physical command. The trace displays source, recipient, timestamp, validity and receipt. A static table must convey the same result without JavaScript.

**L03 Lotus decision lab:** The visitor sets a *digital-only* expected learning gain, cost, protected old-skill delta, and action. The response is one of STAGE_DIGITAL, HOLD, VETO or UNRESOLVED, with an explanation and explicit rule. A rejected physical-care request remains rejected regardless of the gain. Source: Anaiero's existing Lotus and Root metrics.

**L04 Supervisor care lab:** Show a **synthetic** environmental sensor receipt with valid and stale/forged alternatives. The supervisory console can enter a fault-latched state and display HUMAN_REVIEW. It does not connect to a real incubator or feed pump. A clear legend distinguishes Milk/survival from Honey/task feedback.

## Stage B — specialist chip galleries

**L05 Metroidvania director:** CE-D01 through CE-D06 provide application structure, but the current public repository contains proposal manifests rather than a working browser generator. Build a seedable, deterministic room/ability graph first; then show a map, lock/key viability, scene transitions and tested path. The user can inspect the generated world as a graph and see rejected invalid seeds. A full live game renderer is a later optional extension.

**L06 Music/AHD–ARD:** CE-D07 through CE-D09 define an exploratory score/music stack. Start with a visible piano-roll or note/event chart generated from owned example input; show original harmonic/rhythmic movement and parameter effects. Only enable sound playback for project-owned or public-domain music and generated original assets with documented rights. Do not claim this is a trained living neural composer.

**L07 Scientific theorist:** CE-D11/CE-D12 present a constrained "study question → hypotheses → known evidence → discriminating tests → uncertainty" pipeline. The demo should have a fixed and reviewed source set, not unfettered invented references, and label speculative mechanisms explicitly.

**L08 Book MicroCrystal:** CE-D14 can take a small, rights-cleared illustrative chapter, produce source-grounded Bread lessons, show a retrieval-only answer and a more demanding generalization exercise. That illustrates the difference between access to words and retained competence.

**L09 Energy and bandwidth:** A simple interactive engineering budget for the published hypothetical neural stream (channels × samples × bytes) is a useful, honest example of computational Power. Include bus selection, transport capacity and logging costs only where documented. Physiology uses separately qualified energy/support metrics.

## Shared demo page format

~~~text
StarChip CE-Dxx · Proposal/Software/Synthetic status
Problem and user goal
Inputs (editable with guards)
[Run only if actual deterministic implementation exists]
Output / transcript / graph / generated artifact
What this demonstrates
What it does not demonstrate
Validation: test, baseline, seed, source digest
Related data and module contracts
Original references / rights / WDS
~~~

**Minimum fixture records:** demo_id, source_files, input_data, input_rights, deterministic_seed, expected_output, evaluator/test_id, status, generated_at, runtime_version, source_revision_sha, explanatory_limitations. All offline by default. The site must not fetch private conversations or process arbitrary uploads without clear authorization.

## Demo status and publishing rules

No control should advertise an action that it cannot perform. A live demo must have a passing test and stable fixture so a visitor can reproduce the example. For any demo output, never show unobserved biology as a measured result, and never conflate electrical output from a simulator with a live neuron. A "planned" capability can still have a beautiful process animation, but label it a **conceptual storyboard**.

## What makes this compelling

A visitor can *see* how new capabilities would enter the system, how the Lotus decides whether to stage learning, how a Crystal contains source-linked education, and how the scientific team tests a physiological hypothesis. The same studio bridges game creation, music, research, library knowledge and a future instrument architecture—without removing scientific honesty.

See [site design](03-design-system.md), [content evidence](04-evidence-and-content-model.md) and [delivery roadmap](07-release-plan.md).
