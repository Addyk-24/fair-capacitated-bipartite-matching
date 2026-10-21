# Skill based experimental study

## Purpose and scope

This experiment family evaluates the matching algorithms on eligibility graphs
constructed from the repository's weighted named-skill student–project model. It is a
deterministic synthetic workload, not a real student dataset or an empirical claim about
any institution. The existing Week 4 matrix remains the generic random-utility benchmark
and is not merged with, overwritten by, or described as a skill-model evaluation.

The configured study contains 69 datasets: five controlled experiment families, three
seeds per setting, six registered algorithms, and three timed repeats per algorithm.
This implies 1,242 raw run rows when all configured points complete. This is a design
count, not an experimental result.

## Reproducible workload model

`applications/run_skill_experiments.py` produces source `students.csv` and `projects.csv`
files, invokes the existing `convert_student_projects.py`, and saves the resulting
common-format instances. Its SplitMix64 integer generator and SHA-256 seed derivation
avoid dependence on Python's `random` stream. Student and project streams are separated
so controlled sweeps reuse comparable profile and requirement prefixes. The run config,
manifest, source CSVs, instances, seeds, and generated parameters are retained.

Students draw from five named skill tracks plus shared skills. A student's group selects
their primary track; within-track skills are sampled with probability 0.76, shared
skills with probability 0.58, and other-track skills with the experiment's
`cross_track_probability`. The student skill count is bounded by the configured minimum
and maximum. Project requirements contain 5–7 named skills by default, with positive
weights sampled from 5–35, 1–3 mandatory skills, and a minimum coverage threshold. The
converter decides eligibility and utility exactly as documented in the application
specification. Total capacity is `ceil(agents * capacity_ratio)`, distributed across
projects using deterministic integer weights and largest remainders.

These choices create varied project coverage, skill overlap, mandatory bottlenecks,
group composition, and capacity pressure. They are explicit modeling assumptions; they
do not replace validation on a representative real dataset.

## Experiment matrix

| Experiment | Controlled change | Fixed or seeded conditions | Main questions |
| --- | --- | --- | --- |
| `skill_scaling_agents` | 100, 250, 500, 1,000 students | 50 projects, capacity ratio 1.0, 35% threshold | How do runtime and allocation outcomes scale with student count? |
| `skill_scaling_projects` | 10, 25, 50, 100 projects | 500 students, capacity ratio 1.0, 35% threshold | How do runtime, opportunity, and outcomes change with project count? |
| `skill_capacity` | Total seat/student ratios 0.50, 0.75, 1.0, 1.25, 1.50 | 500 students, 50 projects, 35% threshold | How do capacity constraints affect feasible assignments, utility, and seat use? |
| `skill_minimum_coverage` | 20%, 35%, 50%, 65% | 500 students, 50 projects, capacity ratio 1.0 | How does stricter skill coverage filter eligibility and feasible assignments? |
| `skill_specialization` | Cross-track skill probability 0.08, 0.20, 0.35; group skew 0.0, 0.8 | 500 students, 50 projects, capacity ratio 1.0 | How sensitive are outcomes to profile overlap and unequal group composition? |

Every condition uses seeds 17, 29, and 43. Student and project profiles are paired by
seed where the controlled variables permit it. The same generated instance is passed to
every algorithm. Matching is repeated three times to summarize runtime; deterministic
allocation outputs should remain identical across repeats.

## Algorithms and comparison rules

The driver runs every registered algorithm: `greedy_reference`, `cardinality`,
`weighted`, `capacitated_flow`, `capacitated_flow_maxcard`, and `fairness`.

- Assignment coverage is compared with the maximum capacity-feasible cardinality
  reported by `matching_runner`. Capacity-one `cardinality` is compared to this value
  only when every project has capacity at most one. `weighted` has no cardinality
  guarantee and is not given a count-optimality ratio.
- Total utility ratios use `capacitated_flow` as the maximum-total-utility reference.
  `greedy_reference` is reported as a heuristic against this objective. `weighted` is
  directly comparable only on capacity-one instances. The max-cardinality and fair
  algorithms optimize other primary objectives, so their utility is shown descriptively
  and not labeled as a same-objective approximation ratio.
- Fairness is the established minimum assigned utility divided by 100, with an empty
  allocation scored zero. The fair algorithm maximizes count, then minimum utility,
  then total utility. Group labels describe generated populations; they do not change
  the fairness objective and no group-parity guarantee is claimed.
- Capacity utilization is assigned students divided by total seats. Eligibility
  opportunity is reported using eligible-edge density, isolated agents/projects, and
  maximum feasible cardinality.

## Outputs

Use a new run id for each run. The driver refuses to overwrite an existing generated
dataset or raw-results namespace.

```text
data/skill_based/<run-id>/source/       generated application CSVs
data/skill_based/<run-id>/instances/    converted common-format datasets
data/skill_based/<run-id>/manifest.json seeds, parameters, and relative dataset paths
results/metrics/skill_based/<run-id>/   raw runner CSVs and params.txt per dataset
results/tables/skill_based/<run-id>/    per-dataset and aggregated CSV/Markdown tables
results/figures/skill_based/<run-id>/   300-dpi PNG and vector PDF/SVG figures
```

Analysis checks validity, common instance hashes, repeat stability, and the expected
algorithm set. A failed integrity check returns a non-zero exit code and records the
problems beside the tables. Figures show seed-level variability; repeated runtime rows
are reduced to their median for each dataset first. No results are included until the
commands below are run.

## Commands

From the repository root in Windows PowerShell, choose an unused run id and execute in
this order:

```powershell
$runId = "skill_based_v1_20261021"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
python applications/run_skill_experiments.py --phase generate --config experiments/configs/skill_based_v1.json --run-id $runId
python applications/run_skill_experiments.py --phase run --config experiments/configs/skill_based_v1.json --run-id $runId --runner .\build\matching_runner.exe
python notebooks/analyze_skill_results.py --run-id $runId
```

If `$runId` already exists under `data/skill_based`, `results/metrics/skill_based`, or
the analysis output folders, the relevant step stops instead of overwriting. Change the
run id for a fresh independent run. Install the packages in `requirements.txt` if
matplotlib, NumPy, or pandas are unavailable. These commands do not invoke the generic
`run_week4.py` pipeline.
