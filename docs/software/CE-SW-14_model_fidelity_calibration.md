---
title: "CE-SW-14 — SIM3.2 Model Fidelity, Calibration Evidence and Frozen Holdout Evaluations"
project: "ChristEye Anaiero"
version: "0.1.0"
status: "Executable synthetic-only evaluation; not independently validated biology"
related: "CE-SW-06, CE-SW-12, CE-SW-13, CE-SW-11"
---

# CE-SW-14 — SIM3.2 model fidelity and calibration evidence

## Scope

SIM3.2 adds **read-only evaluation of committed CEVRUN/1 colony histories** against an immutable reference dataset. The evaluator verifies a versioned `CELAW/1` *declarative law profile*, an exact SHA-256-pinned `CEVOBS/1` observation dataset, and a sealed `CEVEVAL/1` evaluation bundle. It refuses to compare reference values with a different source, configuration, or law version. It does not modify the runner's cells, processes, molecular fields, checkpoints or lineage records.

`CELAW/1` describes the nine bounded, existing native SIM3.1 law parameters, **not arbitrary executable plugins**. This preserves cross-platform deterministic behavior and avoids loading untrusted code into the living-machine runner. Every parameter must be supplied, distinct, versioned and equal to the run's configured values; if the model rules change, create a new law version and code implementation. Law hashes identify a software model, not proven physical kinetic parameters.

Scientific status: the included demonstration only compares predictions to **synthetic, deliberately offset reference values**. Passing does not establish physiological accuracy, genotype-to-phenotype prediction, molecular training transfer or independent experimental replication. The `independent_holdout_verified=false` field is mandatory until provenance checks can actually verify independently acquired measurements. Source-material hashes identify byte content, not genotype or living-cell properties.

## Portable, sealed file formats

**`CELAW/1` — versioned symbolic law profile:**

```text
CELAW/1
name sim3-default
domain SYNTHETIC_ONLY
version 1
config_sha256 <run-config-digest>
parameter food_per_cell 1
parameter fed_gain 1
parameter unfed_cost 7
parameter upkeep_cost 1
parameter stress_cutoff 20
parameter stress_cost 4
parameter maturity_gain 1
parameter blood_step 1
parameter learning_gain 1
end
seal <sha256-of-all-previous-UTF8-bytes>
```

**`CEVOBS/1` — a pinned reference dataset:**

```text
CEVOBS/1
name synthetic-test-reference
origin SYNTHETIC_FIXTURE
config_sha256 <run-config-digest>
source_sha256 <material-provenance-digest>
units DIMENSIONLESS_PROXY
row 0 all alive 3 1 train
row 1 all alive 3 1 train
row 3 all alive 4 1 holdout
end
seal <sha256-of-preseal-text>
```

Each `row` has: **synthetic tick**, colony ID (`all` or a known colony), metric, observed integer, positive absolute uncertainty radius, and frozen partition (`train` or `holdout`). Valid metrics are `alive`, `energy`, `learning`, `food`, `stress`, and `maturity`. Values are dimensionless proxies. Duplicate `(tick, colony, metric)` records are rejected; each holdout colony/metric must also appear in training. A real scientific dataset needs separately qualified instrument metadata, calibration, sample identity, uncertainty budget and ethics/privacy review; importing arbitrary real observations is intentionally excluded from this first release.

**`CEVEVAL/1` — sealed evaluation request:**

```text
CEVEVAL/1
name sim3-baseline-evaluation
domain SYNTHETIC_ONLY
config_sha256 <run-config-digest>
law_file examples/sim3/baseline.celaw <law-file-SHA256>
observations examples/sim3/synthetic-reference.cevobs <dataset-file-SHA256>
end
seal <sha256-of-preseal-text>
```

The resolver enforces relative paths under the operator-approved artifact root, checks the raw file SHA-256 before parsing, and prevents path traversal/symlink escapes. Limits include **512 rows** and a **4096-tick observation horizon**. It requires every checkpoint from tick 0 through the latest observation tick to exist and pass the existing SIM3 SHA-256/state decoder. No interpolated values, invented time points or partially reconstructed lineages are accepted.

## Calibration and held-out evaluation procedure

1. **Freeze** the `CEVOBS/1` rows and their `train`/`holdout` labels before computing predictions. Hash and seal the file, then reference its exact SHA-256 in `CEVEVAL/1`. The parser checks labels and mapping, but cannot independently establish the experimental independence of a dataset.
2. **Pin** the `CELAW/1` profile to the complete runner config SHA-256, law version and nine exact parameter values. The evaluator does not fit law parameters from holdout data and cannot execute arbitrary plug-in code.
3. **Audit the committed history:** retrieve every `tick-NNNNNN.state` file from tick zero through maximum requested tick, verify its checksum and configuration, and then calculate each requested aggregate from the actual state. Cells remain distinguishable by colony, including lineage tombstones, which are counted separately from living cells.
4. **Compute residuals** `prediction - observation` and report training and holdout `MAE`, `RMSE`, signed bias, and coverage of each reference's declared **absolute proxy interval**. The interval radius is not a Gaussian standard deviation, and coverage is not a confidence interval.
5. **Derive training-only discrepancy radii:** compute nearest-rank 90th percentiles of absolute training residuals, both pooled and **separately for each metric**, so high-magnitude energy/food proxies do not hide failures on learning/viability. Apply the frozen radii to holdout observations and report their coverage per metric and globally. These are not validated 90% predictive intervals, especially for small or nonrepresentative samples.
6. **Retain provenance:** report embeds configuration, source-material, model/law, dataset and evaluation manifest hashes. Optional `MODEL_EVALUATION` WindStore registration stores the report object and emits a simulated-only receipt. Reports cannot alter the source machine or historical observations.

The inclusion of a held-out partition is a software test of evaluation discipline; independent experimental validation would require measurement provenance, independent specimens/conditions, calibrated instrument uncertainty, prespecified training procedures, repeated measurements and independent replication.

## CLI

```sh
cmake -S native -B build/native -DCE_WIND_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# Start from a verified existing SIM3 culture or generate a new history:
ce_colony_runner step examples/connected-gardens.cev ./fidelity-run 6

# Do not overwrite the source culture. A new report file is required.
ce_colony_fidelity evaluate \
  examples/connected-gardens.cev ./fidelity-run \
  examples/sim3/baseline.ceveval . model-fidelity-report.json

# Optional durable WindStore event (not a scientific approval):
ce_colony_fidelity evaluate \
  examples/connected-gardens.cev ./fidelity-run \
  examples/sim3/baseline.ceveval . model-fidelity-indexed.json ./registry
```

The simulation law profile must describe the existing run exactly. To compare different law settings, create separate, pinned CEVRUN configurations, produce separate histories, then freeze matching observational reference datasets. Model selection must use the training partition only; do not rank candidate settings by holdout performance and call the same holdout an unbiased test. A future SIM3.3 may automate a prespecified bounded search and evaluation on a fully untouched, independently curated external dataset.

## Native and cross-platform acceptance

- SHA-256 bitwise validation of config, law, observations, and evaluation manifests, with strict grammar and data limits.
- In-process test covering 28 frozen synthetic rows (12 training, 16 holdout), the known residual metrics and stability across repeated reads.
- Fail-closed tests for changed file bytes, wrong law parameter, malformed law IDs, invalid plan paths, duplicate ticks/metric rows, missing checkpoint and altered checkpoint hashes.
- End-to-end CLI CTest: run native colony, produce report, optionally index into durable WindStore, ensure the report cannot be silently overwritten.
- Python JSON Schema/fixture tests and optional hosted Windows/Linux compiler and sanitizer checks.
- Scientific labels are carried through every report. This is not a protocol for wet-lab manipulation, cell reprogramming, pathogen creation, tissue culture feeding, stimulation, or biological performance inference.
