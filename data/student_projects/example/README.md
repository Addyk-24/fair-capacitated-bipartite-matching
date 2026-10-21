# Student–project example dataset

These small, hand-authored CSVs are the reproducible application example. Skill
fields are comma-separated named categories (never numeric codes). A student is
eligible only if their skills include every required project skill. Preferred
skill matches affect utility but never eligibility.

Run from the repository root:

```bash
python applications/convert_student_projects.py data/student_projects/example data/synthetic/student_project_example
build/matching_runner --input data/synthetic/student_project_example --algo all --output results/metrics/student_project_example.csv
```

The converter sorts skills and input records deterministically and writes the
common `agents.csv`, `resources.csv`, and sorted `edges.csv` format. Utility is
`clamp(1, 100, round(100 * (0.70 * required_match + 0.30 * preferred_match)))`;
required_match is the fraction of required skills present, with an empty required
set scoring 1, and preferred_match is the fraction of preferred skills present,
with an empty preferred set scoring 0. Eligibility requires required_match = 1.
Thus every eligible edge has integer utility 70–100 (or 100 if both sets empty).
