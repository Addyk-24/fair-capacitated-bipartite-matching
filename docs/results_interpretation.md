# Week 4 experimental results

**Run status:** completed on 2026-10-21 with the integrated `cardinality` and
`fairness` stages. The five checked-in Week 4 configs produced 4,428 raw rows across
222 datasets and six algorithms. The experiment runner reported 0 invalid runs; the
analysis integrity check reported all rows valid, identical instance hashes across
algorithms on each dataset, and stable allocation outcomes across repeats.

Derived tables are in `results/tables/`; plots are in `results/figures/`. Exact per-run
measurements are in `results/metrics/`. The configs, algorithms, seed values, and
analysis script are the source of record; runtime values depend on the machine and
system load.

## Objectives and fair comparisons

The stages solve different problems, and the analysis leaves objective-incompatible
ratio cells blank:

- `capacitated_flow` maximizes total utility using full resource capacities.
- `capacitated_flow_maxcard` maximizes assignment count, then total utility, using
  full capacities.
- `fairness` maximizes assignment count, then the minimum utility among assigned
  agents, then total utility. It matched the max-cardinality count on all 222 datasets.
- `weighted` and `cardinality` treat resources as capacity one. They are directly
  comparable to capacity-aware stages only when all capacities equal one.
- `greedy_reference` is a heuristic with full capacity support; its utility and
  coverage ratios are reported against their corresponding capacity-aware optima.

The fairness score is the minimum assigned utility divided by 100. Its mean was **0.263**
across the matrix, compared with **0.233** for `capacitated_flow_maxcard`. The fairness
allocation's mean total utility was 98.5% of the maximum-total-utility allocation when
shown as a descriptive trade-off; its lowest per-dataset ratio was 87.7%. These utility
ratios describe the efficiency cost of the fairness objective and are not presented as
an optimization guarantee or a same-objective comparison.

## Findings

### Capacity-one instances

There were five capacity-one datasets in the capacity experiment. `weighted`,
`capacitated_flow`, `capacitated_flow_maxcard`, `fairness`, and `greedy_reference` all
matched the same total utility on each. This cross-check compares the exact methods on
the same feasible set; it also shows that the fairness tie-break preserves the utility
optimum on these cases.

### Capacity and coverage

Across the full matrix, `capacitated_flow` assigned an average of **439.5** agents per
dataset; `capacitated_flow_maxcard` and `fairness` each assigned **455.6**. The utility
optimizer can leave lower-value assignments unused when that produces a higher total
utility. The max-cardinality and fairness stages instead preserve the greatest feasible
coverage.

In the capacity experiment, the capacity-aware utility solver averaged 486.2 assigned
agents and utility 30,059.4 under balanced capacity. The max-cardinality solver assigned
all 500, with average utility 29,777.6. The fairness solver also assigned all 500 and
averaged utility 29,631.8. At high capacity, utility and maximum-cardinality allocations
nearly coincide (utilities 32,807.2 and 32,802.4 respectively).

The capacity-one `weighted` stage assigned 25 agents in all capacity regimes because it
uses at most one seat per each of the 25 resources. Its utility ratio is reported only
for capacity-one cases. The `cardinality` stage likewise has a meaningful count ratio
only when the actual capacities are all one.

### Density and eligibility

With 500 agents and balanced capacity, the utility-maximizing flow stage assigned an
average of 288.3 agents at density 0.05, 393.9 at 0.1, 491.8 at 0.3, and 499.6 at 0.6.
Sparse eligibility, rather than total nominal capacity alone, limits achievable
coverage at low density.

### Heuristic and runtime

Across datasets where comparison is meaningful, `greedy_reference` averaged 0.949 of
the capacity-aware maximum utility and 0.914 of maximum coverage. Its median runtime was
0.235 ms. The capacity-aware utility and max-cardinality flow stages had median runtimes
of 32.610 ms and 33.611 ms. The fairness stage was slower: median **262.034 ms**, with a
maximum observed runtime of **17,862.877 ms**. Its threshold search repeatedly solves
flow instances, so this cost is expected to exceed a single flow optimization.

Log-log fits over the six tested sizes gave the following agent-scaling exponents:

| Algorithm | Empirical exponent | R² |
| --- | ---: | ---: |
| `greedy_reference` | 1.126 | 0.997 |
| `cardinality` | 0.974 | 0.989 |
| `weighted` | 1.622 | 0.976 |
| `capacitated_flow` | 1.904 | 0.995 |
| `capacitated_flow_maxcard` | 1.951 | 0.997 |
| `fairness` | 1.930 | 0.999 |

These are measured trends for this generated workload, not proofs of worst-case
complexity. See [algorithm_complexity.md](algorithm_complexity.md) for theoretical
bounds. Timings are especially sensitive to graph density, capacities, and machine load.

## Group sweep limitation

`week4_groups` varies the number of labels and group-size skew, but the approved
fairness objective is individual max-min utility and does not use `group_id`. Its score
therefore measures the worst assigned individual utility, not demographic or group
parity. The experiment does not establish group-fairness guarantees. A group-parity
claim would require a separately approved metric and optimization objective.

## Reproduction

From the repository root on Windows PowerShell:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
python experiments/run_week4.py --suite build/experiment_suite.exe --overwrite
```

The script runs every `experiments/configs/week4_*.cfg`, then rebuilds the tables and
figures with `notebooks/analyze_results.py`. Use `--overwrite` to replace existing raw
experiment CSVs.
