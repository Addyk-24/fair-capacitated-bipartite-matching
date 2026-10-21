"""Convert named-skill student/project CSVs into the common matching dataset."""
import argparse
import csv
import os


def skills(value):
    return {part.strip() for part in value.split(",") if part.strip()}


def read_rows(path):
    with open(path, encoding="utf8", newline="") as stream:
        return list(csv.DictReader(stream))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", help="directory containing students.csv and projects.csv")
    parser.add_argument("output", help="common-format dataset directory to create")
    args = parser.parse_args()
    students = sorted(read_rows(os.path.join(args.source, "students.csv")), key=lambda row: row["student_id"])
    projects = sorted(read_rows(os.path.join(args.source, "projects.csv")), key=lambda row: row["project_id"])
    if len({row["student_id"] for row in students}) != len(students) or len({row["project_id"] for row in projects}) != len(projects):
        raise ValueError("student_id and project_id values must be unique")
    groups = sorted({row["group"] for row in students})
    group_id = {group: i for i, group in enumerate(groups)}
    student_skills = [skills(row["skills"]) for row in students]
    required = [skills(row["required_skills"]) for row in projects]
    preferred = [skills(row["preferred_skills"]) for row in projects]
    os.makedirs(args.output, exist_ok=True)
    with open(os.path.join(args.output, "agents.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["agent_id", "group_id"])
        w.writerows((i, group_id[row["group"]]) for i, row in enumerate(students))
    with open(os.path.join(args.output, "resources.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["resource_id", "capacity"])
        w.writerows((i, int(row["capacity"])) for i, row in enumerate(projects))
    with open(os.path.join(args.output, "edges.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["agent_id", "resource_id", "utility"])
        for i, have in enumerate(student_skills):
            for j, needs in enumerate(required):
                if not needs.issubset(have):
                    continue
                req_match = len(needs & have) / len(needs) if needs else 1.0
                pref_match = len(preferred[j] & have) / len(preferred[j]) if preferred[j] else 0.0
                utility = max(1, min(100, int(100 * (0.70 * req_match + 0.30 * pref_match) + 0.5)))
                w.writerow([i, j, utility])
    with open(os.path.join(args.output, "config.txt"), "w", encoding="utf8") as f:
        f.write("source=student-project named-skill CSV\n")
        f.write("utility=round(100*(0.70*required_match+0.30*preferred_match)), clamped to 1..100\n")
        f.write("group_labels=" + ",".join(groups) + "\n")


if __name__ == "__main__":
    main()
