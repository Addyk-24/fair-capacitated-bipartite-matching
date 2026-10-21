"""Convert weighted-skill student/project CSVs into the common matching dataset."""
import argparse
import csv
import json
import math
import os

DEFAULT_MINIMUM_COVERAGE = 35.0
MAX_MANDATORY_SKILLS = 3


def read_rows(path):
    with open(path, encoding="utf8", newline="") as stream:
        return list(csv.DictReader(stream))


def parse_student_skills(value, student_id):
    result = {part.strip() for part in (value or "").split(",") if part.strip()}
    if len(result) != len([part for part in (value or "").split(",") if part.strip()]):
        raise ValueError(f"student {student_id!r} has duplicate skill names; list each skill once")
    return result


def parse_project(row):
    project_id = row.get("project_id", "<unknown>")
    raw = row.get("skills", "")
    try:
        skill_list = json.loads(raw)
    except (TypeError, json.JSONDecodeError) as exc:
        raise ValueError(f"project {project_id!r}: skills must be a JSON array of objects") from exc
    if not isinstance(skill_list, list) or not skill_list:
        raise ValueError(f"project {project_id!r}: skills must be a non-empty JSON array")
    normalized = []
    names = set()
    mandatory_count = 0
    for index, item in enumerate(skill_list):
        if not isinstance(item, dict) or set(item) != {"name", "weight", "mandatory"}:
            raise ValueError(f"project {project_id!r}: skill {index + 1} must contain exactly name, weight, mandatory")
        name = item["name"].strip() if isinstance(item["name"], str) else ""
        if not name:
            raise ValueError(f"project {project_id!r}: skill {index + 1} needs a non-empty name")
        if name in names:
            raise ValueError(f"project {project_id!r}: duplicate skill {name!r}; keep one definition")
        names.add(name)
        if isinstance(item["weight"], bool):
            weight = math.nan
        else:
            try:
                weight = float(item["weight"])
            except (TypeError, ValueError):
                weight = math.nan
        if not math.isfinite(weight) or weight <= 0:
            raise ValueError(f"project {project_id!r}: weight for {name!r} must be finite and positive")
        if not isinstance(item["mandatory"], bool):
            raise ValueError(f"project {project_id!r}: mandatory for {name!r} must be true or false")
        mandatory_count += int(item["mandatory"])
        normalized.append((name, weight, item["mandatory"]))
    if mandatory_count > MAX_MANDATORY_SKILLS:
        raise ValueError(f"project {project_id!r} has {mandatory_count} mandatory skills; maximum is {MAX_MANDATORY_SKILLS}. Mark nonessential skills optional or revise the project.")
    try:
        total = math.fsum(weight for _, weight, _ in normalized)
    except OverflowError as exc:
        raise ValueError(f"project {project_id!r}: total skill weight overflowed; use smaller finite relative weights") from exc
    if not math.isfinite(total) or total <= 0:
        raise ValueError(f"project {project_id!r}: total skill weight must be finite and positive")
    raw_threshold = row.get("minimum_coverage", "")
    if raw_threshold.strip() == "":
        threshold = DEFAULT_MINIMUM_COVERAGE
    else:
        try:
            threshold = float(raw_threshold)
        except (TypeError, ValueError):
            threshold = math.nan
    if not math.isfinite(threshold) or not 0 <= threshold <= 100:
        raise ValueError(f"project {project_id!r}: minimum_coverage must be a finite number from 0 to 100")
    return normalized, total, threshold


def convert(source, output):
    students = read_rows(os.path.join(source, "students.csv"))
    projects = read_rows(os.path.join(source, "projects.csv"))
    for row in students:
        if not row.get("student_id", "").strip() or not row.get("group", "").strip():
            raise ValueError("each student row needs non-empty student_id and group fields")
    for row in projects:
        if not row.get("project_id", "").strip():
            raise ValueError("each project row needs a non-empty project_id")
    students.sort(key=lambda row: row["student_id"])
    projects.sort(key=lambda row: row["project_id"])
    if len({row["student_id"] for row in students}) != len(students) or len({row["project_id"] for row in projects}) != len(projects):
        raise ValueError("student_id and project_id values must be unique")
    groups = sorted({row["group"] for row in students})
    group_id = {group: i for i, group in enumerate(groups)}
    student_skills = [parse_student_skills(row.get("skills", ""), row["student_id"]) for row in students]
    project_data = [parse_project(row) for row in projects]
    capacities = []
    for row in projects:
        try:
            capacity = int(row["capacity"])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"project {row['project_id']!r}: capacity must be a non-negative integer") from exc
        if capacity < 0 or str(capacity) != row["capacity"].strip():
            raise ValueError(f"project {row['project_id']!r}: capacity must be a non-negative integer")
        capacities.append(capacity)

    os.makedirs(output, exist_ok=True)
    with open(os.path.join(output, "agents.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["agent_id", "group_id"])
        w.writerows((i, group_id[row["group"]]) for i, row in enumerate(students))
    with open(os.path.join(output, "resources.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["resource_id", "capacity"])
        w.writerows((i, capacities[i]) for i in range(len(projects)))
    edge_count = 0
    with open(os.path.join(output, "edges.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["agent_id", "resource_id", "utility"])
        for i, have in enumerate(student_skills):
            for j, (skill_list, total, threshold) in enumerate(project_data):
                if any(mandatory and name not in have for name, _, mandatory in skill_list):
                    continue
                matched = math.fsum(weight for name, weight, _ in skill_list if name in have)
                coverage = 100.0 * matched / total
                if coverage + 1e-12 < threshold:
                    continue
                utility = min(100, max(0, math.floor(coverage + 0.5)))
                w.writerow([i, j, utility])
                edge_count += 1
    with open(os.path.join(output, "config.txt"), "w", encoding="utf8") as f:
        f.write("source=student-project weighted-skill CSV v1\n")
        f.write("eligibility=all mandatory skills and weighted coverage >= project minimum_coverage\n")
        f.write(f"default_minimum_coverage={DEFAULT_MINIMUM_COVERAGE:g}\n")
        f.write(f"maximum_mandatory_skills_per_project={MAX_MANDATORY_SKILLS}\n")
        f.write("utility=round-half-up(100 * matched project weight / total project weight), range 0..100\n")
        f.write("weights=positive relative values normalized by each project's total weight\n")
        f.write(f"eligible_edges={edge_count}\n")
        f.write("group_labels=" + ",".join(groups) + "\n")
    return edge_count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", help="directory containing students.csv and projects.csv")
    parser.add_argument("output", help="common-format dataset directory to create")
    args = parser.parse_args()
    try:
        convert(args.source, args.output)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
