# Experimental Results: Interpretation (preliminary)

**Status of this document.** Written from one full run of the Week 4 matrix on 2026-10-21.
It covers the four stages that are merged on `main`: `greedy_reference`, `weighted`,
`capacitated_flow` and `capacitated_flow_maxcard`. Two things are **not** in it yet:

- the **cardinality** stage (not merged),
- any **fairness** result (no fairness metric is wired in, so `fairness_score` is `NA` and the
  fairness-vs-utility plot is not produced).

Rerun `experiments/run_week4.py` once those land and update the numbers and the last section.
Runtimes below are from one laptop (Windows, MinGW GCC 6.3, `-O3`); the *shape* of the curves
matters, not the absolute milliseconds. Utility, assigned counts and ratios do not depend on the
machine.

## 1. What was run

Five experiments, 2,952 raw rows, 222 datasets x 4 algorithms, every run validated
(eligible edges only, one assignment per agent, capacities respected, totals consistent).

| Experiment | Varies | Fixed |
| :--- | :--- | :--- |
| `week4_scaling` | agents 100 to 4000 | 25 resources, density 0.1, balanced capacity |
| `week4_scaling_resources` | resources 10 to 400 | 1000 agents, density 0.1, balanced capacity |
| `week4_capacity` | capacity regime (fixed 1, 2, 4; low, balanced, high) | 500 agents, 25 resources, density 0.2 |
| `week4_utility_density` | utility distribution x density 0.05 to 0.6 | 500 agents, 25 resources, balanced capacity |
| `week4_groups` | group count, group-size skew, popularity skew, capacity | 500 agents, 25 resources (for the fairness stage) |

Data checks done by `notebooks/analyze_results.py` before any number is used: all rows valid;
within each dataset every algorithm received the identical instance (same `instance_hash`);
every algorithm gave identical results on all repeats (they are deterministic).

Reference for the ratios: **utility** is divided by `capacitated_flow` (the best total utility
achievable under the real capacities); **assigned count** is divided by
`capacitated_flow_maxcard` (the largest possible number of assigned agents).

## 2. Findings

### 2.1 The stages only agree where they solve the same problem (capacity 1)

At capacity 1 (`fixed1`, 5 datasets) `weighted`, `capacitated_flow` and
`capacitated_flow_maxcard` return the **same total utility on every dataset**. Two independently
written exact algorithms (Hungarian vs min-cost flow) agreeing exactly is the strongest
correctness evidence in the project. (`greedy_reference` also equals the optimum there, which
is plausible with 500 agents competing for 25 resources: few agents are the best choice for two
resources at once.)

### 2.2 `weighted` is a capacity-1 stage, and the numbers show what that costs

`weighted` assigns at most one agent per resource, so with 25 resources it assigned exactly 25
agents in every regime. Its utility relative to the optimum falls as capacity grows:

| Capacity | weighted utility / optimum |
| :--- | :--- |
| fixed 1 | 1.000 |
| fixed 2 | 0.513 |
| fixed 4 | 0.267 |
| low (about 0.5 x agents in total) | 0.118 |
| balanced | 0.072 |
| high | 0.066 |

This is the expected `~1/c` behaviour, not a defect in the Hungarian implementation: the
problem it is asked to solve is smaller. It means `weighted` must only be compared with
capacity-aware stages at capacity 1; elsewhere the gap measures the value of spare capacity.
The overall "mean utility ratio 0.134" for `weighted` in the summary table mixes regimes and
should not be quoted on its own.

### 2.3 Coverage versus utility inside the capacity-aware stages

The two flow variants optimise different objectives:

- `capacitated_flow` maximises utility and may leave agents unassigned.
- `capacitated_flow_maxcard` assigns as many agents as possible first, then maximises utility.

Under **balanced** capacity (total capacity equal to the number of agents), on average
`capacitated_flow` assigns 486 of 500 agents and reaches utility 30,059, while
`capacitated_flow_maxcard` assigns all 500 and reaches 29,778 (**0.9% less utility** for 14
more agents placed). Across all datasets `capacitated_flow` left some agents unassigned in 155 of
222, with as little as 88.8% of the achievable count placed; the most utility the full-coverage
variant gave up was **12.3%** (1000 agents, 50 resources, balanced, uniform utilities). When
capacity is ample (`high`) the two nearly coincide (500 vs 499.4 agents, utility ratio 1.000).

Why it matters for fairness: leaving the lowest-value agents out is the efficient choice, so
the efficiency-maximising allocation systematically drops whoever has low utility
for the scarce resources. How that is distributed over groups is exactly what the fairness stage
has to measure; the `week4_groups` experiment generates the datasets for it, but no group-level
conclusion can be drawn until a fairness metric is wired in.

### 2.4 The greedy reference is fast but loses utility when capacity is tight

`greedy_reference` runs in under 5 ms on every dataset but reaches only 0.949 of the optimum on
average (worst case 0.876) and assigns 91.4% of the achievable count. It matches the optimum on
only 7 of 222 datasets. Its loss is largest at balanced capacity (mean ratio 0.934) and vanishes
at capacity 1 and 2 (1.000, 0.999). It is useful as a lower bound, not as a stage.

### 2.5 Runtime growth

Empirical growth exponents from a log-log fit of mean runtime (6 sizes, R-squared at least 0.99):

| Algorithm | exponent in agents | exponent in resources |
| :--- | :--- | :--- |
| greedy_reference | 1.15 | 1.00 |
| weighted | 1.30 | 0.92 |
| capacitated_flow | 1.99 | 0.98 |
| capacitated_flow_maxcard | 1.97 | 0.92 |

- `capacitated_flow` grows roughly **quadratically in agents** (0.38 ms at 100 agents, 626 ms at
  4000). The documented worst case is O(F x V x E) with F, V and E all growing with the number of
  agents; the measured quadratic growth is consistent with F x E (flow value times edges) because
  the shortest-path searches rarely touch the whole graph.
- `weighted` grows far more slowly than its documented O(k^2 l) bound would allow: going from 10
  to 400 resources (40x) took 4.4 ms to 128 ms (29x), where a quadratic-in-k term would predict
  1,600x. These instances are random and sparse, so the bound is not reached; this says nothing
  about worst-case inputs. The weighted time also includes converting the instance to the
  solver's own graph format (the adapter), and its matrix construction is O(E(A+R)).
- These are **measured trends on random instances over the sizes tested**, not a verification of
  the worst-case bounds. The bounds themselves are argued in the algorithm documents.

### 2.6 Effect of utility distribution and density

Density decides how many agents can be placed at all under balanced capacity. The optimal
utility stage (`capacitated_flow`) assigned on average 288 of 500 agents at density 0.05, 394 at
0.1, 492 at 0.3 and 500 (499.6) at 0.6, so at low density many agents simply have no
eligible resource with room.

`greedy_reference` stays below the optimum at every density and distribution tested, by between
2.4% (low-utility distribution, density 0.05) and 11.6% (high-utility distribution, density 0.1).
The gap is smallest for the `low` distribution (0.94 to 0.98 of optimum) and largest for `high`
and `uniform` at mid densities (0.88 to 0.92). The full tables are
`results/tables/utility_density_effect.md` and `results/tables/per_dataset.csv`.

## 3. Anomalies and limits

- **Greedy equals optimum at capacity 1.** Not an anomaly (see 2.1), but it means capacity-1 data
  cannot discriminate between greedy and the exact stages at these agent-to-resource ratios.
- **Capacity regimes `low`/`balanced`/`high` are defined from the agent count.** Total capacity is
  about 0.5x / 1x / 2x the number of agents, so `balanced` is close to the point where full
  coverage first becomes possible; that is why the coverage-versus-utility gap peaks there.
- **Seeds.** 3 to 5 seeds per setting; the plots show the spread of runtime over seeds but
  utility ratios are averaged without confidence intervals. More seeds would be needed before
  claiming differences smaller than about 1%.
- **Machine dependence.** Timing was taken on one Windows laptop with a laptop's normal
  background load; repeat on a quiet machine before quoting absolute times in the report.
- **Not yet covered:** cardinality stage; fairness score and the fairness-vs-utility trade-off.

## 4. Reproducing everything

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build
python experiments/run_week4.py --suite build/experiment_suite      # use --overwrite to rerun
```

Raw rows: `results/metrics/<experiment>/...` (never edited). Derived: `results/tables/` and
`results/figures/`. Files shipped in this repository: the five `experiments/configs/week4_*.cfg`,
`experiments/run_week4.py` and `notebooks/analyze_results.py`.
