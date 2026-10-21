# Common Dataset Format and Algorithm Interface

This is the single input/output contract for every algorithm stage (cardinality,
weighted, capacitated flow, fairness). Do not invent member-specific formats.

## 1. Problem instance

| Symbol | Meaning | Field |
| :--- | :--- | :--- |
| `U` | agents `0 .. A-1` | `Instance::num_agents` |
| `V` | resources `0 .. R-1` | `Instance::num_resources` |
| `E ⊆ U×V` | eligible pairs; a pair not listed is **not eligible** | `Instance::edges` |
| `w_uv` | utility of edge `(u,v)` (integer, higher is better) | `Edge::utility` |
| `c_v` | capacity of resource `v`, integer `>= 0` | `Instance::capacity[v]` |
| `g(u)` | group label of agent `u`, in `0 .. G-1` | `Instance::group_id[u]` |

Rules:

- Each agent is assigned to **at most one** resource.
- Resource `v` receives **at most `c_v`** agents. Capacity 1 gives classic matching.
- Capacity and group are stored once per resource / agent, never per edge.
- Edges are sorted by `(agent_id, resource_id)` with no duplicate pairs.
- Generated utilities are integers in `[utility_min, utility_max]` (default 1..100).
  Zero is allowed; the readers do not restrict the sign of a utility, so algorithms
  that cannot handle negative utilities must say so in their own documentation.
- Total utility of an allocation is the sum of `w_uv` over the assigned pairs.

Isolated agents (no eligible resource) and zero-capacity resources are valid input.

## 2. On-disk format

A dataset is a **directory** with three CSV files (header row required, `,` separator,
LF or CRLF line endings, blank lines ignored):

`agents.csv`

```text
agent_id,group_id
0,0
1,0
2,1
```

`resources.csv`

```text
resource_id,capacity
0,1
1,2
```

`edges.csv`

```text
agent_id,resource_id,utility
0,0,10
0,1,5
```

- `agent_id` and `resource_id` in `agents.csv` / `resources.csv` must count up from 0
  with no gaps; the row order is the id.
- `edges.csv` must be sorted by `(agent_id, resource_id)`.
- `num_groups` is recovered as `max(group_id) + 1`.
- An optional `config.txt` (`key=value` lines) records how a synthetic dataset was
  generated, including the seed. Readers ignore it.

`readInstance` rejects anything that violates these rules with a message naming the
file and line.

## 3. Algorithm interface

```cpp
// include/common.hpp
MatchingResult run<Name>(const daa::Instance& inst);
```

Return a `MatchingResult` with:

| Field | Meaning |
| :--- | :--- |
| `assignments` | list of `(agent_id, resource_id)`, each an edge of the instance |
| `total_assigned_agents` | must equal `assignments.size()` |
| `total_utility` | must equal the sum of the assigned edges' utilities |
| `algorithm_name` | set by the runner from the registered name |
| `runtime_milliseconds` | **set by the runner**; algorithms do not time themselves |
| `fairness_score` | `-1` until a fairness metric is wired in (Week 3) |

The runner calls `validateResult` after every run (eligible edges only, agent used at
most once, capacities respected, totals consistent) and records a `valid` flag.

To make an algorithm runnable by name:

1. Put the implementation in `algorithms/<name>.cpp` (CMake picks it up on the next
   configure).
2. Declare it in `include/algorithms.hpp`.
3. Add one `registerAlgorithm("<name>", run<Name>);` line to `registerBuiltinAlgorithms()`
   in `src/registry.cpp`.

## 4. Raw results file

`matching_runner` appends one row per (dataset, algorithm, repeat) to a CSV
(default `results/metrics/runs.csv`):

```text
dataset,algorithm,repeat,agents,resources,edges,assigned_count,total_utility,fairness_score,runtime_ms,valid,instance_hash
```

- `fairness_score` is `NA` until a fairness metric is set (see hooks below).
- `valid` is `1` only if the algorithm did not throw and every result check passed.
- `instance_hash` is a 64-bit fingerprint of the exact `Instance` the algorithm received
  (`include/fingerprint.hpp`). Rows of the same dataset must all carry the same hash; that is
  the proof that every algorithm was compared on identical input.
- Raw files are never edited by hand; plots and summaries are derived into separate files.

## 4a. Runner hooks (`include/experiment.hpp`)

| Hook | Purpose |
| :--- | :--- |
| `addResultCheck(name, fn)` | extra rule run on every result after the built-in `validateResult`; return `""` if fine, else a message. A failure marks the run invalid as `"<name>: <message>"`. For stage-specific rules, e.g. a fairness constraint. |
| `setFairnessMetric(fn)` | computes `fairness_score` for every valid run (unless the algorithm already set it). The metric is chosen by the fairness stage; until then the column stays `NA`. |

An algorithm that throws is caught and logged as an invalid run instead of aborting the experiment.

## 4b. Capacity semantics per stage

| Stage | Uses resource capacity `c_j` |
| :--- | :--- |
| `greedy_reference` | fully (up to `c_j` agents per resource) |
| `weighted` (Hungarian, capacity-1) | at most **one** agent per resource, whatever `c_j` is; resources with `c_j = 0` are never used |
| capacitated flow | fully (min-cost max-flow) |
| fairness-aware | per its own formulation |

Consequence for experiments: do not compare `weighted` against capacity-aware stages on
instances where capacity exceeds 1 and read the gap as an algorithm difference. With
`capacity_mode = fixed` and `capacity = 1` all stages face the same problem and are
directly comparable. In other regimes the gap measures the value of spare capacity.

## 5. Synthetic generator

`dataset_generator` produces the format above from a seed. The same options and seed
give the same dataset on every compiler and OS (all randomness is integer-based and
implemented in `include/rng.hpp`, not `<random>`).

| Option | Effect |
| :--- | :--- |
| `--agents`, `--resources` | sizes |
| `--density p` | probability that a pair is eligible |
| `--popularity-skew s` | `0`: uniform. Larger `s`: resource `j` becomes eligible with probability `p·w_j`, `w_j ∝ 1/(j+1)^s` (mean 1, capped at 1), so a few resources are contested |
| `--utility-dist` | `uniform`, `bell` (mean of 3 draws), `low` (min of 3), `high` (max of 3) on `[--utility-min, --utility-max]` |
| `--capacity-mode` | `low` / `balanced` / `high`: total capacity ≈ 0.5 / 1 / 2 × agents, spread evenly with the remainder on random resources; `fixed` with `--capacity K` gives every resource `K`. Generated capacities are never below 1 |
| `--groups g`, `--group-skew s` | agents sampled into `g` groups; `s=0` equal sizes, larger `s` makes group 0 dominate |
| `--seed` | RNG seed |

Draw order is groups, then capacities, then edges (agent-major), so a parameter change
only affects the parts that depend on it.

## 6. Experiment suite (`experiment_suite`)

Runs a whole experiment from one config file: dataset sweep x algorithms x repeats.

```text
experiment_suite --config experiments/configs/smoke.cfg [--out DIR] [--name N] [--overwrite] [--dry-run] [--list]
```

Config format: `key = value` lines, `#` comments, **every parameter may be a comma-separated list**
and the suite runs the full cross product (`agents = 100,500` and `seed = 1,2,3` give 6 datasets).

| Key | Meaning (default) |
| :--- | :--- |
| `name` | experiment name, results folder (`experiment`) |
| `algos` | comma-separated names, or `all` (`all`) |
| `repeats` | runs per algorithm per dataset, one raw row each (`1`) |
| `agents`, `resources`, `density`, `popularity_skew`, `utility_dist`, `capacity_mode`, `groups`, `group_skew` | generator parameters, as in section 5 |
| `capacity` | values for `capacity_mode = fixed` only |
| `utility_min`, `utility_max` | utility range (not swept) |
| `seed` | list of seeds; each seed is a separate dataset |
| `out_dir`, `dataset_dir`, `save_datasets`, `overwrite` | output locations; `save_datasets = true` also writes each dataset in the section 2 format |

Unknown keys, duplicate keys, bad values and duplicate datasets in a sweep are errors, and every
generator config is validated before anything runs.

Output layout, grouped by input size and then by parameter setting:

```text
<out_dir>/<name>/config_used.cfg                       copy of the config that produced the results
<out_dir>/<name>/n<agents>_m<resources>/<tag>/runs.csv raw rows (section 4)
<out_dir>/<name>/n<agents>_m<resources>/<tag>/params.txt   generator parameters, seed, instance_hash, algorithms
```

`<tag>` encodes the remaining parameters, e.g. `d0.15_ps1_ubell_clow_g3_gs1_seed1`
(density, popularity skew, utility distribution, capacity, groups, group skew, seed).

All algorithms and repeats of a dataset receive the same `const Instance`. Existing `runs.csv`
files are never replaced silently; pass `--overwrite` (or `overwrite = true`) to allow it.

Shipped configs: `smoke.cfg` (runs under `ctest` as `smoke_suite`) and `week3_scaling.cfg`.
