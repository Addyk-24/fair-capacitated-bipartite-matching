# Student–project application model

The matching algorithms stay domain-independent: they receive indexed agents,
resources, capacities, eligible edges, and integer utilities. The application converter
maps students to agents and projects to resources. It performs all skill-based
eligibility filtering before an algorithm optimizes an allocation.

## Input tables

`students.csv` has `student_id`, `group`, and `skills`. `skills` is a comma-separated
list of skill names.

`projects.csv` has `project_id`, non-negative integer `capacity`, optional
`minimum_coverage`, and `skills`. The `skills` field is a JSON array embedded in the
CSV cell. Each object contains exactly:

```json
{"name":"Python","weight":35,"mandatory":true}
```

Skill names are semantic labels, compared as exact strings after trimming whitespace.
Project skill names must be unique and every project must define at least one skill.
Weights must be positive and finite. They are relative values, so their total may be
any finite positive value; the converter normalizes by that project's total weight.
`mandatory` must be a JSON boolean. At most three skills per project may be mandatory.
Configurations beyond this cap are rejected with an actionable error; no skills are
silently demoted.

`minimum_coverage` is a percentage from 0 through 100, inclusive. An omitted or empty
value uses the default **35**. The converter rejects non-finite or out-of-range values.

## Eligibility and utility

A student qualifies for a project only if they possess **every mandatory skill** and
their weighted skill coverage is at least the project's minimum coverage. Thus mandatory
qualifications and the coverage threshold are separate conditions. Ineligible pairs are
omitted from the common graph before matching begins.

For a student `s`, project `p`, and skill `k`, let `a_sk` be 1 when the student's named
skill set contains `k` and 0 otherwise. Let `w_pk` be the project's skill weight divided
by the sum of all its skill weights. The utility is:

```text
U(s,p) = round_half_up(100 * sum_k(w_pk * a_sk))
```

Equivalently, with unnormalized positive weights, divide the matched weight sum by the
total project weight. The integer score is clamped only for floating-point boundary
rounding and lies in **[0, 100]**. Mandatory skills contribute their full weight to
coverage as well as enforcing eligibility. A threshold of zero can produce an eligible
edge with utility zero; that edge remains present and is distinct from a missing edge.

### Worked example

Suppose a project has Python (35, mandatory), SQL (25, mandatory), Statistics (25,
optional), and Data Visualization (15, optional). Its total weight is 100. A student
with Python, SQL, and Statistics meets both mandatory qualifications and has coverage
`(35+25+25)/100 = 85%`, so utility is **85** and the student is eligible at the default
35% threshold. A student with Python and Statistics has 60% coverage but lacks
mandatory SQL, so the pair is ineligible despite its high coverage.

## Groups, capacities, and objectives

Each student's source `group` is retained for general evaluation metadata. Distinct
group labels are sorted lexicographically and mapped to contiguous integer ids, making
conversion reproducible. The selected fairness objective does not use group parity:
it maximizes assignment count, then the minimum utility among assigned students, then
total utility. An empty allocation has fairness score zero.

Project capacities are carried directly into the common `resources.csv` representation.
Students are not forced into assignments. Before optimization, `matching_runner`
reports eligible edge count, students with no eligible projects, projects with no
eligible students, and the maximum feasible assignment cardinality under capacities.
This distinguishes insufficient capacity or eligibility from an optimization failure.

## Reproducible conversion

From the repository root:

```bash
python applications/convert_student_projects.py data/student_projects/example data/synthetic/student_project_example
build/matching_runner --input data/synthetic/student_project_example --algo all --output results/metrics/student_project_example.csv
```

The checked-in example is the source of record. Conversion sorts student and project
records by identifier, maps group labels deterministically, evaluates named skills
without numeric encodings, and writes the common `agents.csv`, `resources.csv`, and
`edges.csv` files. Converter validation errors identify the project and offending field.
