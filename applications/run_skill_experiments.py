"""Generate reproducible weighted-skill student-project instances and run matching algorithms.

The generator uses a fixed SplitMix64 stream and stores the full source CSVs, converted
instances, config snapshot, and dataset manifest. It refuses to overwrite an existing
run id. Example:
  python applications/run_skill_experiments.py --phase generate --config experiments/configs/skill_based_v1.json --run-id skills_v1
  python applications/run_skill_experiments.py --phase run --config experiments/configs/skill_based_v1.json --run-id skills_v1 --runner build/matching_runner.exe
"""
import argparse
import csv
import hashlib
import itertools
import json
import math
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "applications"))
from convert_student_projects import convert  # noqa: E402

TRACKS = {
    "analytics": ["Python", "SQL", "Statistics", "Data Cleaning", "Visualization", "Experiment Design", "Pandas", "Dashboards"],
    "software": ["Java", "C++", "Algorithms", "Unit Testing", "APIs", "Version Control", "Design Patterns", "Code Review"],
    "systems": ["Linux", "Networking", "Databases", "Cloud", "Security", "Containers", "Distributed Systems", "Monitoring"],
    "business": ["Requirements", "Project Planning", "Process Mapping", "Finance", "Communication", "Presentation", "Documentation", "Stakeholder Analysis"],
    "research": ["Literature Review", "Survey Design", "Causal Inference", "Research Methods", "Data Ethics", "Technical Writing", "Reproducibility", "Evaluation"]
}
SHARED_SKILLS = ["Teamwork", "Problem Solving", "Git", "Technical Communication"]
TRACK_NAMES = list(TRACKS)
MAX_MANDATORY_SKILLS = 3


class SplitMix64:
    """Small fixed integer PRNG; avoids dependence on Python's random implementation."""
    MASK = (1 << 64) - 1

    def __init__(self, seed):
        self.state = seed & self.MASK

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & self.MASK
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & self.MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & self.MASK
        return (z ^ (z >> 31)) & self.MASK

    def below(self, bound):
        if bound <= 0:
            raise ValueError("random bound must be positive")
        return self.next() % bound

    def integer(self, low, high):
        return low + self.below(high - low + 1)

    def shuffle(self, values):
        values = list(values)
        for i in range(len(values) - 1, 0, -1):
            j = self.below(i + 1)
            values[i], values[j] = values[j], values[i]
        return values

    def choose(self, values):
        return values[self.below(len(values))]

    def weighted_index(self, weights):
        total = sum(weights)
        point = self.below(total)
        for i, weight in enumerate(weights):
            if point < weight:
                return i
            point -= weight
        raise AssertionError("weighted selection fell through")


def stable_seed(seed, experiment, parameters):
    payload = json.dumps([seed, experiment, parameters], sort_keys=True, separators=(",", ":"))
    digest = hashlib.sha256(payload.encode("utf8")).digest()
    return int.from_bytes(digest[:8], "little")


def skill_catalog():
    catalog = [(name, track) for track, names in TRACKS.items() for name in names]
    catalog.extend((name, "shared") for name in SHARED_SKILLS)
    return catalog


def validate_parameters(p):
    for key in ("agents", "projects", "groups", "student_skill_min", "student_skill_max",
                "project_skill_min", "project_skill_max", "mandatory_min", "mandatory_max"):
        if not isinstance(p.get(key), int) or isinstance(p.get(key), bool) or p[key] < 1:
            raise ValueError(f"{key} must be a positive integer")
    if p["student_skill_min"] > p["student_skill_max"] or p["student_skill_max"] > len(skill_catalog()):
        raise ValueError("student skill bounds must be ordered and fit the skill catalog")
    if p["project_skill_min"] > p["project_skill_max"] or p["project_skill_max"] > len(skill_catalog()):
        raise ValueError("project skill bounds must be ordered and fit the skill catalog")
    if p["project_skill_min"] < p["mandatory_min"]:
        raise ValueError("project_skill_min must be at least mandatory_min")
    if p["groups"] > len(TRACK_NAMES):
        raise ValueError(f"groups must not exceed {len(TRACK_NAMES)} available skill tracks")
    if not 1 <= p["mandatory_min"] <= p["mandatory_max"] <= MAX_MANDATORY_SKILLS:
        raise ValueError(f"mandatory_min/max must satisfy 1 <= min <= max <= {MAX_MANDATORY_SKILLS}")
    for key in ("capacity_ratio", "minimum_coverage", "group_skew", "cross_track_probability"):
        value = p.get(key)
        if not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError(f"{key} must be finite")
    if p["capacity_ratio"] <= 0:
        raise ValueError("capacity_ratio must be positive")
    if not 0 <= p["minimum_coverage"] <= 100:
        raise ValueError("minimum_coverage must be in [0,100]")
    if p["group_skew"] < 0:
        raise ValueError("group_skew must be non-negative")
    if not 0 <= p["cross_track_probability"] <= 1:
        raise ValueError("cross_track_probability must be in [0,1]")


def group_probabilities(count, skew):
    # Integer weights make assignment deterministic and the first group progressively
    # more common as skew grows. The documented sweep uses skew 0 and 0.8.
    return [max(1, int(round(1_000_000 / ((i + 1) ** skew)))) for i in range(count)]


def student_skill_set(rng, track, p, catalog):
    selected = set()
    for name, category in catalog:
        if category == "shared":
            probability = 0.58
        elif category == track:
            probability = 0.76
        else:
            probability = p["cross_track_probability"]
        if rng.below(1_000_000) < int(probability * 1_000_000):
            selected.add(name)
    minimum, maximum = p["student_skill_min"], p["student_skill_max"]
    if len(selected) < minimum:
        missing = [name for name, _ in catalog if name not in selected]
        selected.update(rng.shuffle(missing)[:minimum - len(selected)])
    if len(selected) > maximum:
        selected = set(rng.shuffle(sorted(selected))[:maximum])
    return sorted(selected)


def project_skill_set(rng, p, catalog):
    track = rng.choose(TRACK_NAMES[:p["groups"]])
    count = rng.integer(p["project_skill_min"], p["project_skill_max"])
    primary = [name for name, category in catalog if category == track]
    secondary = [name for name, category in catalog if category != track]
    primary_count = min(len(primary), max(1, (count * 3 + 4) // 5))
    names = rng.shuffle(primary)[:primary_count]
    names += rng.shuffle(secondary)[:count - len(names)]
    names = rng.shuffle(names)
    mandatory_count = rng.integer(p["mandatory_min"], min(p["mandatory_max"], count))
    mandatory_names = set(rng.shuffle(names)[:mandatory_count])
    skill_rows = []
    for name in names:
        skill_rows.append({"name": name, "weight": rng.integer(5, 35), "mandatory": name in mandatory_names})
    return skill_rows


def allocate_capacities(rng, agents, projects, ratio):
    total = int(math.ceil(agents * ratio))
    weights = [rng.integer(1, 5) for _ in range(projects)]
    denominator = sum(weights)
    capacities = [(total * weight) // denominator for weight in weights]
    remainders = [(total * weight) % denominator for weight in weights]
    tie_break = {i: rng.next() for i in range(projects)}
    order = sorted(range(projects), key=lambda i: (-remainders[i], tie_break[i]))
    for i in order[:total - sum(capacities)]:
        capacities[i] += 1
    return capacities


def write_source_dataset(directory, p, seed, experiment):
    student_model = {key: p[key] for key in ("groups", "group_skew", "cross_track_probability",
                                                "student_skill_min", "student_skill_max")}
    project_model = {key: p[key] for key in ("groups", "project_skill_min", "project_skill_max",
                                                "mandatory_min", "mandatory_max")}
    student_rng = SplitMix64(stable_seed(seed, "student_profiles", student_model))
    project_rng = SplitMix64(stable_seed(seed, "project_requirements", project_model))
    capacity_rng = SplitMix64(stable_seed(seed, "project_capacities", {"projects": p["projects"]}))
    catalog = skill_catalog()
    group_labels = TRACK_NAMES[:p["groups"]]
    group_weights = group_probabilities(p["groups"], p["group_skew"])
    students = []
    for i in range(p["agents"]):
        group = group_labels[student_rng.weighted_index(group_weights)]
        students.append({"student_id": f"s{i:06d}", "group": group,
                         "skills": student_skill_set(student_rng, group, p, catalog)})
    projects = []
    capacities = allocate_capacities(capacity_rng, p["agents"], p["projects"], p["capacity_ratio"])
    for j in range(p["projects"]):
        projects.append({"project_id": f"p{j:05d}", "capacity": capacities[j],
                         "minimum_coverage": p["minimum_coverage"],
                         "skills": project_skill_set(project_rng, p, catalog)})
    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, "students.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["student_id", "group", "skills"])
        for s in students:
            w.writerow([s["student_id"], s["group"], ",".join(s["skills"])])
    with open(os.path.join(directory, "projects.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["project_id", "capacity", "minimum_coverage", "skills"])
        for project in projects:
            w.writerow([project["project_id"], project["capacity"], project["minimum_coverage"],
                        json.dumps(project["skills"], ensure_ascii=False, separators=(",", ":"))])
    return sum(capacities)


def safe_component(value):
    if isinstance(value, float):
        return ("%.3f" % value).rstrip("0").rstrip(".").replace(".", "p")
    return str(value)


def combinations(mapping):
    keys = sorted(mapping)
    values = [value if isinstance(value, list) else [value] for value in (mapping[k] for k in keys)]
    for items in itertools.product(*values):
        yield dict(zip(keys, items))


def expand_config(cfg, selected_names=None):
    result = []
    common = cfg["common"]
    for experiment in cfg["experiments"]:
        if selected_names and experiment["name"] not in selected_names:
            continue
        for override in combinations(experiment.get("sweep", {})):
            params = dict(common)
            params.update(override)
            validate_parameters(params)
            for seed in cfg["seeds"]:
                result.append((experiment["name"], params, int(seed)))
    if not result:
        raise ValueError("no experiment datasets selected")
    return result


def dataset_tag(params, seed):
    model = json.dumps(params, sort_keys=True, separators=(",", ":"))
    fingerprint = hashlib.sha256(model.encode("utf8")).hexdigest()[:8]
    parts = [f"n{params['agents']}", f"p{params['projects']}",
             f"cap{safe_component(params['capacity_ratio'])}",
             f"cov{safe_component(params['minimum_coverage'])}",
             f"overlap{safe_component(params['cross_track_probability'])}",
             f"gskew{safe_component(params['group_skew'])}", f"seed{seed}", f"cfg{fingerprint}"]
    return "_".join(parts)


def dataset_relpath(experiment, params, seed):
    return os.path.join(experiment, f"n{params['agents']}_p{params['projects']}", dataset_tag(params, seed))


def assert_fresh(paths):
    existing = [path for path in paths if os.path.exists(path)]
    if existing:
        raise ValueError("refusing to overwrite existing run data: " + ", ".join(existing) +
                         ". Choose a new --run-id to preserve prior outputs.")


def generate_phase(args, cfg, datasets):
    data_root = os.path.join(ROOT, "data", "skill_based", args.run_id)
    assert_fresh([data_root])
    os.makedirs(data_root)
    manifest = {"run_id": args.run_id, "seed_algorithm": "SplitMix64", "config": cfg, "datasets": []}
    relative_paths = [dataset_relpath(experiment, params, seed) for experiment, params, seed in datasets]
    if len(relative_paths) != len(set(relative_paths)):
        raise ValueError("experiment sweep contains duplicate dataset paths; remove repeated parameter values")
    for index, (experiment, params, seed) in enumerate(datasets, 1):
        rel = dataset_relpath(experiment, params, seed)
        source = os.path.join(data_root, "source", rel)
        instance = os.path.join(data_root, "instances", rel)
        capacity = write_source_dataset(source, params, seed, experiment)
        edge_count = convert(source, instance)
        with open(os.path.join(instance, "config.txt"), "a", encoding="utf8") as f:
            f.write("generator=run_skill_experiments.py; prng=SplitMix64\n")
            f.write("experiment=" + experiment + "\n")
            f.write("seed=" + str(seed) + "\n")
        manifest["datasets"].append({"experiment": experiment, "parameters": params,
                                     "seed": seed, "path": rel, "total_capacity": capacity,
                                     "eligible_edges": edge_count})
        print(f"[{index}/{len(datasets)}] generated {rel}: {edge_count} eligible edges, capacity {capacity}")
    with open(os.path.join(data_root, "manifest.json"), "w", encoding="utf8") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")
    with open(os.path.join(data_root, "config_snapshot.json"), "w", encoding="utf8") as f:
        json.dump(cfg, f, indent=2, sort_keys=True)
        f.write("\n")
    print(f"Generated source and common-format instances under {data_root}")


def count_edge_isolates(instance_dir, agents, projects):
    incidence_agents = set()
    incidence_projects = set()
    with open(os.path.join(instance_dir, "edges.csv"), encoding="utf8", newline="") as f:
        for row in csv.DictReader(f):
            incidence_agents.add(int(row["agent_id"]))
            incidence_projects.add(int(row["resource_id"]))
    return agents - len(incidence_agents), projects - len(incidence_projects)


def capacity_summary(instance_dir):
    values = []
    with open(os.path.join(instance_dir, "resources.csv"), encoding="utf8", newline="") as f:
        values = [int(row["capacity"]) for row in csv.DictReader(f)]
    return sum(values), max(values, default=0) <= 1


def run_phase(args, cfg, datasets):
    data_root = os.path.join(ROOT, "data", "skill_based", args.run_id)
    manifest_path = os.path.join(data_root, "manifest.json")
    if not os.path.isfile(manifest_path):
        raise ValueError(f"missing generated manifest {manifest_path}; run phase=generate first")
    with open(manifest_path, encoding="utf8") as f:
        manifest = json.load(f)
    if json.dumps(cfg, sort_keys=True) != json.dumps(manifest["config"], sort_keys=True):
        raise ValueError("the supplied config differs from the generated manifest; use the original config snapshot")
    requested = {(e, json.dumps(p, sort_keys=True), s) for e, p, s in datasets}
    manifest_items = [item for item in manifest["datasets"]
                      if (item["experiment"], json.dumps(item["parameters"], sort_keys=True), item["seed"]) in requested]
    if len(manifest_items) != len(requested):
        raise ValueError("generated manifest does not contain every selected experiment point")
    metrics_root = os.path.join(ROOT, "results", "metrics", "skill_based", args.run_id)
    assert_fresh([metrics_root])
    if not os.path.isfile(args.runner):
        raise ValueError(f"matching_runner executable not found: {args.runner}; build the project first")
    algorithms = cfg.get("algorithms", "all")
    repeats = int(cfg.get("repeats", 3))
    os.makedirs(metrics_root)
    expected_algorithms = ["greedy_reference", "cardinality", "weighted", "capacitated_flow",
                           "capacitated_flow_maxcard", "fairness"]
    with open(os.path.join(metrics_root, "run_manifest.json"), "w", encoding="utf8") as f:
        json.dump({"run_id": args.run_id, "repeats": repeats,
                   "algorithms": expected_algorithms,
                   "datasets": [{"experiment": item["experiment"],
                                 "dataset": os.path.basename(item["path"])}
                                for item in manifest_items]}, f, indent=2, sort_keys=True)
        f.write("\n")
    for index, item in enumerate(manifest_items, 1):
        experiment, p, seed = item["experiment"], item["parameters"], item["seed"]
        rel = item["path"]
        instance = os.path.join(data_root, "instances", rel)
        output_dir = os.path.join(metrics_root, rel)
        os.makedirs(output_dir, exist_ok=False)
        raw = os.path.join(output_dir, "runs.csv")
        command = [args.runner, "--input", instance, "--algo", algorithms,
                   "--repeats", str(repeats), "--output", raw]
        print(f"[{index}/{len(manifest_items)}] running {rel}", flush=True)
        completed = subprocess.run(command, cwd=ROOT, text=True, encoding="utf8",
                                   errors="replace", capture_output=True)
        if completed.stdout:
            print(completed.stdout, end="")
        if completed.stderr:
            print(completed.stderr, end="", file=sys.stderr)
        if completed.returncode != 0:
            raise RuntimeError(f"matching_runner failed for {rel} with exit code {completed.returncode}")
        feasibility_match = re.search(r"maximum_feasible_assignments=(\d+)", completed.stdout)
        if not feasibility_match:
            raise RuntimeError(f"matching_runner did not report feasibility for {rel}")
        no_students, no_projects = count_edge_isolates(instance, p["agents"], p["projects"])
        metadata = dict(p)
        total_capacity, capacity_one = capacity_summary(instance)
        metadata.update({"experiment": experiment, "seed": seed, "repeats": repeats,
                         "algorithms": algorithms, "dataset": os.path.basename(instance),
                         "eligible_edges": item["eligible_edges"],
                         "students_without_eligible_projects": no_students,
                         "projects_without_eligible_students": no_projects,
                         "maximum_feasible_assignments": int(feasibility_match.group(1)),
                         "total_capacity": total_capacity, "capacity_one": capacity_one,
                         "source": "synthetic named weighted-skill model",
                         "generator": "run_skill_experiments.py SplitMix64"})
        with open(os.path.join(output_dir, "params.txt"), "w", encoding="utf8") as f:
            for key in sorted(metadata):
                f.write(f"{key}={metadata[key]}\n")
    print(f"Raw skill-based results saved under {metrics_root}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--phase", choices=("generate", "run"), required=True,
                        help="generate source/instances first, then run algorithms")
    parser.add_argument("--config", required=True, help="JSON skill-based study config")
    parser.add_argument("--run-id", required=True, help="new output namespace; existing ids are never overwritten")
    parser.add_argument("--experiment", action="append", help="limit to one or more configured experiment names")
    parser.add_argument("--runner", help="matching_runner executable (required for --phase run)")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.run_id):
        parser.error("--run-id may contain only letters, digits, underscores, and hyphens")
    if args.phase == "run" and not args.runner:
        parser.error("--runner is required for --phase run")
    try:
        config_path = args.config if os.path.isabs(args.config) else os.path.join(ROOT, args.config)
        with open(config_path, encoding="utf8") as f:
            cfg = json.load(f)
        if not cfg.get("seeds") or not cfg.get("experiments"):
            raise ValueError("config must define non-empty seeds and experiments")
        if int(cfg.get("repeats", 0)) < 1:
            raise ValueError("repeats must be positive")
        if cfg.get("algorithms", "all") != "all":
            raise ValueError("the configured study comparison requires algorithms=all")
        if any(not isinstance(seed, int) or isinstance(seed, bool) or seed < 0 for seed in cfg["seeds"]):
            raise ValueError("seeds must be non-negative integers")
        if len(set(cfg["seeds"])) != len(cfg["seeds"]):
            raise ValueError("seeds must be unique")
        names = [item.get("name") for item in cfg["experiments"]]
        if len(set(names)) != len(names) or any(not isinstance(name, str) or not re.fullmatch(r"skill_[a-z0-9_]+", name) for name in names):
            raise ValueError("experiment names must be unique and use the skill_ prefix")
        datasets = expand_config(cfg, set(args.experiment or []))
        if args.phase == "generate":
            generate_phase(args, cfg, datasets)
        else:
            runner = args.runner if os.path.isabs(args.runner) else os.path.join(ROOT, args.runner)
            args.runner = runner
            run_phase(args, cfg, datasets)
    except (OSError, ValueError, KeyError, RuntimeError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
