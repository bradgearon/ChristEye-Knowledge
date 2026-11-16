---
title: "CE-SW-15 — SIM3.3 Frozen Model Selection and Independent Final Test"
project: "ChristEye Anaiero"
version: "1.0-digital-reference"
status: "Executable C++20 synthetic-only selection engine; scientific validation pending"
related: "CE-SW-12, CE-SW-13, CE-SW-14, CE-SW-11"
---

# CE-SW-15 — SIM3.3: prespecified model selection and untouched final test

## Purpose

SIM3.2 scored one committed virtual-colony history against a frozen dataset. SIM3.3 compares multiple **predeclared symbolic-law profiles** on the *same training observations*, locks the lowest training-error candidate, then evaluates **only the locked winner** against a separate test file. SIM3.3 does not train living tissue, emulate biological memory formation, infer genotype/phenotype or authorize stimulation or material delivery. The executable law profiles are a bounded list of nine numeric SIM3 parameters; the runner never loads arbitrary shared libraries, scripts, genetic sequences or physical-device commands.

The adjective *independent* describes the **software partition**, not independent biological evidence. The demonstration datasets are both constructed synthetic fixtures and share a modeling source; they are deliberately not externally measured or statistically independent biological samples. Every final report states `independent_holdout_verified: false` and `classification: SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION`.

## Frozen manifests and provenance

A `CEVSELECT/1` search manifest is sealed by SHA-256 and pins the training file, a **reserved test SHA-256 (without exposing a test path to selection)**, and up to 16 candidate runner configurations and `CELAW/1` descriptors:

```text
CEVSELECT/1
name model-law-selection
domain SYNTHETIC_ONLY
source_sha256 <source-provenance-sha256>
horizon 6
train examples/sim3/model-search-train.cevsplits <training-sha256>
reserved_test_sha256 <untouched-test-sha256>
candidate baseline examples/connected-gardens-select-baseline.cev <config-sha256> examples/sim3/select-baseline.celaw <law-sha256>
candidate fast-learning examples/connected-gardens-select-fast-learning.cev <config-sha256> examples/sim3/select-fast-learning.celaw <law-sha256>
end
seal <sha256-of-preseal-bytes>
```

`CEVSPLIT/1` describes a **TRAIN_ONLY** or **TEST_ONLY** synthetic observation set. Each row has `tick colony metric observed radius`; duplicate (tick, colony, metric) keys are rejected. Test metrics must be present in training, and no test observation key may duplicate a training key. Allowed metrics: alive, energy, maturity, learning, food and stress; values and radius are abstract dimensionless proxies.

`CELAW/1` binds the complete runner configuration hash and law values. For every candidate, the engine checks the SHA-256 of the exact config file, verifies all referenced source profiles and machine images through `Configuration::load`, and validates a separate pinned `CELAW/1`. To avoid confounding law comparisons, candidates must share material-source identity and identical *synthetic topology* (colonies, seeds, sources, processes and intercolony links). Only their law parameters may differ.

The runner loads a separate committed history directory for each candidate and checks **every** `history/tick-NNNNNN.state` checkpoint from zero through the prespecified horizon. Each checkpoint must deserialize under that candidate's exact config hash and contain the corresponding tick. The history tip SHA-256 is frozen in the winner receipt. No synthetic or real data are interpolated to fill missing checkpoints.

## Selection and validation protocol

1. `prepare` executes each candidate simulator to the same horizon, saving immutable history and resuming an incomplete run if the configuration still matches. It does not open the test file.
2. `select` parses the sealed manifest and **training dataset only**. For each row, compute a normalized absolute error `abs(predicted - observed)/declared_radius`. Ranking is deterministic using an integer fixed-point score (scale 10⁶, half-up rounding per row) to reduce cross-platform differences. It is a *selection heuristic*, not a statistical likelihood or calibrated uncertainty estimate.
3. Rank by sum of scaled normalized errors; ties break by candidate ID in lexical order. Emit a sealed `CEVWINNER/1` file containing the manifest hash, training hash, reserved test digest, winner, each candidate's training score, config hash, law hash and committed history-tip hash.
4. `final` replays **selection using training only**, requiring its canonical output to byte-match the sealed winner file. Only then does it open the separately supplied **TEST_ONLY** file and verify its reserved SHA-256 and lack of training-row overlap. It computes errors for the single winner. Other candidates are **never scored on final-test records by the selection code**.
5. Emit `CEVFINAL/1` JSON with model, provenance hashes, raw and radius-normalized MAE, declared-band coverage and every final-test residual. Its `independent_holdout_verified` remains false until qualified external measurements, independent cohort selection and protected data stewardship are in place. The optional WindStore record uses `MODEL_EVALUATION` with `SIMULATED` origin, not a scientific approval receipt.

The code does not overwrite previously generated winner or final report files. It checks exact content hashes and refuses edited, inconsistent, missing or disconnected candidate histories. A later protocol should add signed/third-party timestamped commitments, strict access separation for confidential final-test data, repeated-cohort testing, uncertainty calibration and multiple-testing controls.

## Example run

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release --target ce_colony_runner ce_colony_select ce_colony_selection_tests
ctest --test-dir build/native -C Release --output-on-failure -R sim33_

# 1. Build both frozen synthetic histories; no holdout file is opened.
ce_colony_select prepare examples/sim3/model-search.cevselect . ./selection-runs

# 2. Rank against TRAIN_ONLY and seal the winner before opening the test data.
ce_colony_select select examples/sim3/model-search.cevselect . ./selection-runs chosen.cevwinner

# 3. Open the reserved TEST_ONLY dataset and score the winner exactly once.
ce_colony_select final examples/sim3/model-search.cevselect . ./selection-runs \
  chosen.cevwinner examples/sim3/model-search-test.cevsplits final-result.json

# Optional WindStore archive of a completed SIMULATED-only report:
ce_colony_select final examples/sim3/model-search.cevselect . ./selection-runs \
  chosen.cevwinner examples/sim3/model-search-test.cevsplits final-registered.json ./registry
```

For demonstrations, `examples/sim3/model-search-winner.sample.cevwinner` and `model-search-final.sample.json` are committed **test fixtures**, so their test data are necessarily public. Do not treat these sample fixtures as a scientific blind study. In an actual independent evaluation, a designated custodian would withhold and attest the final dataset until the selection report is signed and reviewed.

## Validation contracts

- **Reproducibility:** same sealed manifest + candidate configs + checkpoint histories + training rows generate byte-identical locked winner and final report.
- **No test access during model selection:** `prepare`/`select` accept no test-file path. The negative test attempts selection while the final test path does not exist and confirms selection remains valid.
- **Holdout contamination protection:** changed reserved-test hash, mismatched test bytes, duplicate train/test keys, missing metric families and altered locked winner are rejected.
- **Candidate integrity:** changed config or law bytes, invalid law declarations, nonidentical colony topology or missing/corrupt snapshots are rejected before publishing a winner.
- **Strict schema:** JSON Schema validates `CEVFINAL/1` and explicitly forbids `independent_holdout_verified: true` or undeclared physical actuation fields.
- **Platform:** GCC, Clang, and Windows/Linux CMake/CTest workflows; AddressSanitizer/UBSan on supporting host builds.

## Known limitations and next steps

This is a bounded brute-force search over previously enumerated law profiles, not a biologically faithful physics engine or general optimizer. The baseline training fixture intentionally prefers the baseline profile while the test fixture was constructed from a different synthetic law. The reported holdout performance is descriptive only; a low test error would not establish meaningful biological prediction.

Next: **SIM3.4** should add separately governed measurement intake with qualified calibration and uncertainty records, encrypted/restricted sensitive source-data handling, traceable model assumption revisions and statistical methods appropriate for externally acquired observations. No software score alone can establish knowledge transfer through cells, RNA, proteins or Crystal candidates.
