# Student–project example dataset

This deterministic example uses named student skills and a JSON weighted-skill list
for each project. The project schema and formula, eligibility rules, 35% default
coverage threshold, three mandatory-skill cap, and conversion commands are documented
in [the application specification](../../../../docs/specifications/student_project_application.md).

Convert and run from the repository root:

```bash
python applications/convert_student_projects.py data/student_projects/example data/synthetic/student_project_example
build/matching_runner --input data/synthetic/student_project_example --algo all --output results/metrics/student_project_example.csv
```
