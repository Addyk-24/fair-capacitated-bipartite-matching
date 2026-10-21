# Fair and Capacity-Constrained Bipartite Matching

**Course:** CS3212 – Design and Analysis of Algorithms  
**Department:** Department of Computer Engineering, Vishwakarma Institute of Technology, Pune  
**Faculty Guide:** Dr. Pushkar Shripad Joglekar  
**Language:** C++17

---

## 1. Project Overview

This project investigates generalized bipartite matching from agents to resources under multi-dimensional operational constraints. The implementation is developed in pure C++ across progressive stages:

$$\text{Agents } (U) \longrightarrow \text{Utility } (w_{ij}) \longrightarrow \text{Capacity } (c_j) \longrightarrow \text{Fairness Formulation} \longrightarrow \text{Allocation}$$

### Algorithmic Progression

1. **Maximum-Cardinality Baseline:** Baseline feasibility matching enforcing the one-assignment-per-agent constraint.
2. **Maximum-Weight Matching:** Preference- and utility-aware assignment maximizing total allocation weight.
3. **Capacity-Constrained Matching:** Network flow formulation (Min-Cost Max-Flow) handling multi-capacity resources ($c_j \ge 1$).
4. **Fairness-Aware Optimization:** Constrained or multi-objective optimization balancing efficiency against group or individual fairness metrics.
5. **Empirical Benchmarks:** Scalability profiles, theoretical complexity verification, runtime trade-offs, and allocation distributions.

---

## 2. Team Members & Module Responsibilities

| Roll No. | Member Name | Assigned Module | Working Branch |
| :--- | :--- | :--- | :--- |
| **30** | Nikhil Kardile | Fairness metrics, literature review, metric evaluation | `nikhil/fairness-metrics` |
| **31** | Om Karkele | Cardinality baseline, formal problem modeling, complexity proofs | `om/cardinality-baseline` |
| **33** | Kartik Mandhane | Utility modeling, maximum-weight matching, preference schemas | `kartik/weighted-matching` |
| **35** | Yash Kashid | Resource-capacity modeling, min-cost max-flow network formulation | `yash/capacity-flow` |
| **39** | Aditya Katkar | Repository architecture, dataset generator, benchmark pipeline | `aditya/repo-experiments` |

---

## 3. Repository Layout

```text
include/         shared headers: Instance / MatchingResult (common.hpp), registry, generator, I/O
src/             shared code: generator, dataset I/O, validators, algorithm registry, result logging
algorithms/      one .cpp per algorithm stage (picked up automatically by CMake)
experiments/     runners/ (matching_runner, dataset_generator), configs/
data/synthetic/  generated datasets (ignored by git) + hand-made sample_* datasets (tracked)
tests/           unit/ and integration/ tests (run with ctest)
results/         metrics/ (raw CSV), tables/, plots/, figures/ (not tracked)
docs/            specifications/ (dataset format), proofs/, literature/
notebooks/       plotting and analysis (Python, optional)
```

The input/output contract every algorithm must follow is in
[docs/specifications/dataset_format.md](docs/specifications/dataset_format.md).

---

## 4. Build & Test

### Prerequisites

- A C++17 compiler: GCC 10+, Clang 11+, or MSVC 2019+
- CMake 3.20+
- Git
- Python 3.10+ with `pip install -r requirements.txt` (only for plotting)

### Steps

```bash
# 1. Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 2. Build the library, both command-line tools and the tests
cmake --build build --config Release

# 3. Run the test suite
cd build && ctest --output-on-failure -C Release
```

On Windows with MinGW, use `-G "MinGW Makefiles"` (or Ninja) and keep the repository path
free of spaces if `mingw32-make` complains.

---

## 5. Usage

### Generate a dataset

```bash
./build/dataset_generator --out data/synthetic/n500_m40 \
    --agents 500 --resources 40 --density 0.15 --popularity-skew 1.0 \
    --utility-dist bell --capacity-mode balanced --groups 3 --group-skew 1.0 --seed 7
```

Same options and seed always produce the same dataset. `--help` lists every option.

### Run algorithms

```bash
# Run one algorithm, or a comma-separated list, or "all"
./build/matching_runner --input data/synthetic/sample_tiny --algo greedy_reference
./build/matching_runner --input data/synthetic/n500_m40 --algo all --repeats 5 \
    --output results/metrics/runs.csv
./build/matching_runner --list
```

Each run is validated (eligible edges only, one assignment per agent, capacities
respected, totals consistent) and appended to the results CSV with its wall-clock
runtime. On Windows with MSVC the executables are in `build\Release\`.

### Run a whole experiment

```bash
# Every parameter in the config can be a comma-separated list; the suite runs the
# full cross product on identical datasets and writes results by size and setting.
./build/experiment_suite --config experiments/configs/smoke.cfg --dry-run   # list datasets
./build/experiment_suite --config experiments/configs/week3_scaling.cfg
# -> results/metrics/week3_scaling/n<agents>_m<resources>/<parameter tag>/runs.csv
```

Config keys, output layout and the hooks for extra result checks and the fairness
metric are documented in
[docs/specifications/dataset_format.md](docs/specifications/dataset_format.md).
Existing results are never overwritten unless you pass `--overwrite`.

### Reproduce the Week 4 results

```bash
python experiments/run_week4.py --suite build/experiment_suite   # runs experiments/configs/week4_*.cfg
```

This runs the five Week 4 experiments (scaling in agents and resources, capacity regimes,
utility distribution x density, group distributions), writes raw rows to
`results/metrics/<experiment>/...`, then builds processed tables in `results/tables/` and
figures in `results/figures/` with `notebooks/analyze_results.py` (needs the packages in
`requirements.txt`). The analysis checks first that every run is valid, that all algorithms
received the identical instance, and that results are stable across repeats. The
interpretation of the current results is in
[docs/results_interpretation.md](docs/results_interpretation.md).

---

## 6. Status

| Stage | Owner | State |
| :--- | :--- | :--- |
| Repository, dataset format, generator, runner, validators | Aditya | done |
| Experiment suite (config sweeps, seeds, result layout, hooks) | Aditya | Week 3 done |
| Master experiment matrix, analysis scripts, tables, plots, interpretation | Aditya | Week 4 done for the merged stages; rerun when cardinality and fairness land |
| `greedy_reference` (non-optimal sanity baseline, not a project stage) | Aditya | done |
| Maximum cardinality (`cardinality`) | Om | problem definition on `main`; algorithm not yet merged |
| Maximum weight (`weighted`) | Kartik | merged; runnable through an adapter (capacity-1 stage) |
| Capacity-constrained min-cost flow (`capacitated_flow`, `capacitated_flow_maxcard`) | Yash | merged |
| Fairness-aware stage and metric | Nikhil / team | not yet merged (runner hook ready) |

Contribution rules (branches, commits, pull requests, reviews, code constraints) are in
[CONTRIBUTING.md](CONTRIBUTING.md).
