# Student–project application model

The matching library remains domain-neutral: agents are indexed `0..A-1`, resources
`0..R-1`, and eligibility is represented by the presence of an edge. This application
maps students to projects.

## Source tables

`data/student_projects/example/students.csv` contains unique `student_id`, categorical
`group`, comma-separated named `skills`, and `preferred_skills`. The projects table
contains unique `project_id`, non-negative integer `capacity`, and named
`required_skills` and `preferred_skills`. Skills are sets of text labels; numeric skill
codes and arithmetic over skill identifiers are not used.

Eligibility requires every project required skill to appear in the student's skills.
For an eligible pair, let `r` be the fraction of required skills matched (1 for an empty
required set), and `p` the fraction of preferred skills matched (0 for an empty
preferred set). Integer utility is:

```text
utility = clamp(1, 100, round(100 * (0.70*r + 0.30*p)))
```

The 70/30 weighting gives required skills greater influence while allowing stated
preferences to break ties. Students are assigned to groups by the source `group` field;
group names are sorted lexicographically and mapped to contiguous integer ids. These
groups are available to future group-parity experiments; the selected max-min fairness
objective itself does not use group labels.

## Reproducible conversion

From the repository root, run:

```bash
python applications/convert_student_projects.py data/student_projects/example data/synthetic/student_project_example
build/matching_runner --input data/synthetic/student_project_example --algo all --output results/metrics/student_project_example.csv
```

The converter sorts students and projects by their IDs, iterates their ids in sorted
order, uses deterministic set-based skill comparisons, and writes the common three-CSV
dataset format. The source example is checked in; generated common-format files are
recreated by the command above.
