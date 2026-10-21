"""Integrity-check skill-based matching runs and write report-ready tables and figures.

Outputs are isolated under results/tables/skill_based/<run-id>/ and
results/figures/skill_based/<run-id>/. Existing outputs are never overwritten.
Figures are written as 300-dpi PNG plus vector PDF and SVG.
"""
import argparse
import glob
import math
import os
import shutil
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALGO_ORDER = ["greedy_reference", "cardinality", "weighted", "capacitated_flow",
              "capacitated_flow_maxcard", "fairness"]
PALETTE = {"greedy_reference": "#7f8c8d", "cardinality": "#9467bd", "weighted": "#ff7f0e",
           "capacitated_flow": "#1f77b4", "capacitated_flow_maxcard": "#2ca02c", "fairness": "#d62728"}
EXPERIMENTS = ["skill_scaling_agents", "skill_scaling_projects", "skill_capacity",
               "skill_minimum_coverage", "skill_specialization"]


def read_params(path):
    values = {}
    with open(path, encoding="utf8") as stream:
        for line in stream:
            if "=" in line:
                key, value = line.rstrip("\n").split("=", 1)
                values[key] = value
    return values


def load_results(metrics_root):
    paths = sorted(glob.glob(os.path.join(metrics_root, "*", "*", "*", "runs.csv")))
    frames = []
    for path in paths:
        frame = pd.read_csv(path, dtype={"instance_hash": str})
        params = read_params(os.path.join(os.path.dirname(path), "params.txt"))
        for key, value in params.items():
            if key not in frame.columns:
                frame[key] = value
        frames.append(frame)
    if not frames:
        return pd.DataFrame()
    result = pd.concat(frames, ignore_index=True)
    numeric = ["agents", "projects", "capacity_ratio", "minimum_coverage", "groups", "group_skew",
               "cross_track_probability", "total_capacity", "eligible_edges", "maximum_feasible_assignments",
               "students_without_eligible_projects", "projects_without_eligible_students", "seed"]
    for column in numeric:
        if column in result:
            result[column] = pd.to_numeric(result[column], errors="coerce")
    for column in ("fairness_score", "runtime_ms", "assigned_count", "total_utility", "valid"):
        if column in result:
            result[column] = pd.to_numeric(result[column], errors="coerce")
    return result


def integrity_problems(runs):
    problems = []
    if (runs["valid"] != 1).any():
        problems.append(f"{int((runs['valid'] != 1).sum())} invalid raw result rows")
    hashes = runs.groupby(["experiment", "dataset"])["instance_hash"].nunique()
    if (hashes > 1).any():
        problems.append(f"{int((hashes > 1).sum())} dataset(s) have inconsistent instance hashes")
    stability = runs.groupby(["experiment", "dataset", "algorithm"])[
        ["assigned_count", "total_utility", "fairness_score"]].nunique()
    if ((stability > 1).any(axis=1)).any():
        problems.append(f"{int(((stability > 1).any(axis=1)).sum())} dataset/algorithm results vary across repeats")
    expected = set(ALGO_ORDER)
    for (experiment, dataset), group in runs.groupby(["experiment", "dataset"]):
        present = set(group["algorithm"])
        if present != expected:
            problems.append(f"{experiment}/{dataset} has algorithms {sorted(present)}, expected {sorted(expected)}")
    return problems


def check_run_manifest(runs, manifest_path):
    problems = []
    if not os.path.isfile(manifest_path):
        return [f"missing run manifest {manifest_path}; completeness cannot be verified"]
    import json
    with open(manifest_path, encoding="utf8") as stream:
        manifest = json.load(stream)
    expected_algorithms = set(manifest["algorithms"])
    expected_keys = {(item["experiment"], item["dataset"]) for item in manifest["datasets"]}
    actual_keys = set(zip(runs["experiment"], runs["dataset"]))
    missing = expected_keys - actual_keys
    unexpected = actual_keys - expected_keys
    if missing:
        problems.append(f"{len(missing)} expected dataset(s) have no raw rows")
    if unexpected:
        problems.append(f"{len(unexpected)} unexpected dataset(s) appear in raw results")
    if set(runs["algorithm"]) != expected_algorithms:
        problems.append(f"algorithm set is {sorted(set(runs['algorithm']))}, expected {sorted(expected_algorithms)}")
    for key in expected_keys & actual_keys:
        group = runs[(runs.experiment == key[0]) & (runs.dataset == key[1])]
        for algorithm in expected_algorithms:
            rows = group[group.algorithm == algorithm]
            if len(rows) != int(manifest["repeats"]):
                problems.append(f"{key[0]}/{key[1]}/{algorithm}: found {len(rows)} rows, expected {manifest['repeats']}")
    return problems


def make_per_dataset(runs):
    valid = runs[runs["valid"] == 1].copy()
    keys = ["experiment", "dataset", "agents", "resources", "edges", "algorithm", "seed",
            "projects", "capacity_ratio", "minimum_coverage", "group_skew", "cross_track_probability",
            "eligible_edges", "maximum_feasible_assignments", "total_capacity",
            "students_without_eligible_projects", "projects_without_eligible_students", "capacity_one"]
    keys = [key for key in keys if key in valid.columns]
    grouped = valid.groupby(keys, dropna=False)
    table = grouped.agg(assigned=("assigned_count", "first"), utility=("total_utility", "first"),
                        fairness=("fairness_score", "first"), runtime_ms=("runtime_ms", "median"),
                        repeats=("repeat", "count")).reset_index()
    utility_ref = table[table.algorithm == "capacitated_flow"][["experiment", "dataset", "utility"]].rename(
        columns={"utility": "max_total_utility"})
    table = table.merge(utility_ref, on=["experiment", "dataset"], how="left", suffixes=("", "_ref"))
    table["utility_vs_max_total"] = table["utility"] / table["max_total_utility"].replace(0, np.nan)
    table["utility_vs_max_total_descriptive"] = table["utility_vs_max_total"]
    same_capacity_one = table["capacity_one"].astype(str).str.lower().isin(["true", "1"])
    comparable_utility = table["algorithm"].isin(["greedy_reference", "capacitated_flow"])
    comparable_utility |= (table["algorithm"] == "weighted") & same_capacity_one
    table.loc[~comparable_utility, "utility_vs_max_total"] = np.nan
    table["assigned_vs_feasible"] = table["assigned"] / table["maximum_feasible_assignments"].replace(0, np.nan)
    comparable_count = table["algorithm"].isin(["greedy_reference", "capacitated_flow",
                                                 "capacitated_flow_maxcard", "fairness"])
    comparable_count |= (table["algorithm"] == "cardinality") & same_capacity_one
    table.loc[~comparable_count, "assigned_vs_feasible"] = np.nan
    table["capacity_utilization"] = table["assigned"] / table["total_capacity"].replace(0, np.nan)
    table["eligible_edge_density"] = table["edges"] / (table["agents"] * table["resources"])
    table["feasible_fraction"] = table["maximum_feasible_assignments"] / table["agents"]
    return table


def markdown_table(frame):
    def cell(value):
        if pd.isna(value):
            return ""
        if isinstance(value, (float, np.floating)):
            return f"{value:.3f}"
        return str(value).replace("|", "\\|")
    lines = ["| " + " | ".join(map(str, frame.columns)) + " |",
             "| " + " | ".join("---" for _ in frame.columns) + " |"]
    for _, row in frame.iterrows():
        lines.append("| " + " | ".join(cell(row[column]) for column in frame.columns) + " |")
    return "\n".join(lines) + "\n"


def save_table(frame, root, name):
    frame.to_csv(os.path.join(root, name + ".csv"), index=False)
    with open(os.path.join(root, name + ".md"), "w", encoding="utf8") as stream:
        stream.write(markdown_table(frame))


def summarize(per_dataset):
    summary = per_dataset.groupby(["experiment", "algorithm"]).agg(
        datasets=("dataset", "count"), mean_assigned=("assigned", "mean"),
        mean_assigned_vs_feasible=("assigned_vs_feasible", "mean"),
        mean_total_utility=("utility", "mean"), mean_utility_vs_max_total=("utility_vs_max_total", "mean"),
        mean_min_normalized_utility=("fairness", "mean"), median_runtime_ms=("runtime_ms", "median"),
        p95_runtime_ms=("runtime_ms", lambda x: x.quantile(0.95)),
        mean_capacity_utilization=("capacity_utilization", "mean")).reset_index()
    summary["_order"] = summary.algorithm.map(lambda name: ALGO_ORDER.index(name) if name in ALGO_ORDER else 99)
    return summary.sort_values(["experiment", "_order"]).drop(columns="_order").reset_index(drop=True)


def aggregate(per_dataset, experiment, x):
    subset = per_dataset[per_dataset.experiment == experiment]
    if subset.empty:
        return pd.DataFrame()
    dimensions = x if isinstance(x, list) else [x]
    group = subset.groupby(dimensions + ["algorithm"], dropna=False)
    out = group.agg(mean_assigned=("assigned", "mean"), sd_assigned=("assigned", "std"),
                    mean_utility=("utility", "mean"), sd_utility=("utility", "std"),
                    mean_fairness=("fairness", "mean"), sd_fairness=("fairness", "std"),
                    mean_runtime_ms=("runtime_ms", "mean"), sd_runtime_ms=("runtime_ms", "std"),
                    mean_capacity_utilization=("capacity_utilization", "mean"),
                    sd_capacity_utilization=("capacity_utilization", "std"),
                    mean_eligible_edge_density=("eligible_edge_density", "mean"),
                    mean_feasible_fraction=("feasible_fraction", "mean"),
                    mean_assigned_vs_feasible=("assigned_vs_feasible", "mean"),
                    mean_utility_vs_max_total=("utility_vs_max_total", "mean"),
                    datasets=("dataset", "nunique")).reset_index()
    return out


def finish(fig, figures_dir, name):
    fig.tight_layout()
    fig.savefig(os.path.join(figures_dir, name + ".png"), dpi=300, bbox_inches="tight")
    fig.savefig(os.path.join(figures_dir, name + ".pdf"), bbox_inches="tight")
    fig.savefig(os.path.join(figures_dir, name + ".svg"), bbox_inches="tight")
    plt.close(fig)


def plot_lines(summary, x, y, err, xlabel, ylabel, title, path, logarithmic=False, log_x=False):
    if summary.empty:
        return
    fig, ax = plt.subplots(figsize=(7.2, 4.8))
    for algorithm in ALGO_ORDER:
        rows = summary[summary.algorithm == algorithm].sort_values(x)
        if rows.empty:
            continue
        xs = rows[x].astype(float).to_numpy()
        ys = rows[y].astype(float).to_numpy()
        errors = rows[err].fillna(0).astype(float).to_numpy() if err in rows else np.zeros(len(rows))
        if logarithmic:
            keep = ys > 0
            xs, ys, errors = xs[keep], ys[keep], errors[keep]
        if len(xs) == 0:
            continue
        ax.errorbar(xs, ys, yerr=errors, marker="o", linewidth=1.8, markersize=5, capsize=3,
                    color=PALETTE[algorithm], label=algorithm)
    if logarithmic or log_x:
        ax.set_xscale("log", base=2)
    if logarithmic:
        ax.set_yscale("log")
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.24)
    ax.legend(frameon=False, fontsize=8, ncol=2)
    finish(fig, os.path.dirname(path), os.path.basename(path))


def plot_scalability(per_dataset, experiment, x, name, figures_dir):
    sub = per_dataset[per_dataset.experiment == experiment]
    if sub.empty:
        return
    summary = sub.groupby([x, "algorithm"]).agg(runtime=("runtime_ms", "mean"),
                                                   spread=("runtime_ms", "std")).reset_index()
    plot_lines(summary, x, "runtime", "spread", "Number of students" if x == "agents" else "Number of projects",
               "Median runtime per dataset (ms), mean across seeds", "Skill-based matching scalability",
               os.path.join(figures_dir, name), logarithmic=True)
    effects = aggregate(per_dataset, experiment, x)
    label = "students" if x == "agents" else "projects"
    plot_lines(effects, x, "mean_assigned", "sd_assigned", f"Number of {label}",
               "Assigned students", f"Assignment cardinality by {label} scale",
               os.path.join(figures_dir, f"assigned_by_{label}"), log_x=True)
    plot_lines(effects, x, "mean_utility", "sd_utility", f"Number of {label}",
               "Total utility", f"Total allocation utility by {label} scale",
               os.path.join(figures_dir, f"utility_by_{label}"), log_x=True)


def plot_capacity(per_dataset, figures_dir):
    summary = aggregate(per_dataset, "skill_capacity", "capacity_ratio")
    if summary.empty:
        return
    plot_lines(summary, "capacity_ratio", "mean_assigned", "sd_assigned", "Total seats / students",
               "Assigned students", "Assignment cardinality by capacity", os.path.join(figures_dir, "assignments_by_capacity"))
    plot_lines(summary, "capacity_ratio", "mean_capacity_utilization", "sd_capacity_utilization",
               "Total seats / students", "Occupied seats / available seats", "Capacity utilization",
               os.path.join(figures_dir, "capacity_utilization"))
    plot_lines(summary, "capacity_ratio", "mean_fairness", "sd_fairness", "Total seats / students",
               "Minimum assigned utility / 100", "Max-min utility fairness by capacity",
               os.path.join(figures_dir, "fairness_by_capacity"))
    plot_lines(summary, "capacity_ratio", "mean_utility", "sd_utility", "Total seats / students",
               "Total utility", "Total allocation utility by capacity", os.path.join(figures_dir, "utility_by_capacity"))


def plot_threshold(per_dataset, figures_dir):
    summary = aggregate(per_dataset, "skill_minimum_coverage", "minimum_coverage")
    if summary.empty:
        return
    plot_lines(summary, "minimum_coverage", "mean_assigned", "sd_assigned", "Minimum coverage threshold (%)",
               "Assigned students", "Assignments under eligibility thresholds",
               os.path.join(figures_dir, "assignments_by_coverage_threshold"))
    plot_lines(summary, "minimum_coverage", "mean_fairness", "sd_fairness", "Minimum coverage threshold (%)",
               "Minimum assigned utility / 100", "Fairness as eligibility becomes stricter",
               os.path.join(figures_dir, "fairness_by_coverage_threshold"))
    opportunity = per_dataset[per_dataset.experiment == "skill_minimum_coverage"].drop_duplicates(
        ["dataset", "minimum_coverage"])
    opportunity = opportunity.groupby("minimum_coverage").agg(
        edge_density=("eligible_edge_density", "mean"), edge_density_sd=("eligible_edge_density", "std"),
        eligible_feasible_fraction=("feasible_fraction", "mean"), feasible_fraction_sd=("feasible_fraction", "std"),
        seeds=("dataset", "nunique")).reset_index()
    fig, ax = plt.subplots(figsize=(7.2, 4.8))
    ax.errorbar(opportunity.minimum_coverage, opportunity.edge_density, yerr=opportunity.edge_density_sd.fillna(0),
                marker="o", linewidth=1.8, capsize=3, color="#1f77b4", label="Eligible edge density")
    ax.errorbar(opportunity.minimum_coverage, opportunity.eligible_feasible_fraction,
                yerr=opportunity.feasible_fraction_sd.fillna(0), marker="s", linewidth=1.8, capsize=3,
                color="#d62728", label="Maximum feasible assignments / students")
    ax.set_xlabel("Minimum weighted coverage threshold (%)")
    ax.set_ylabel("Fraction")
    ax.set_title("Eligibility opportunity as coverage becomes stricter")
    ax.set_ylim(0, 1.02)
    ax.grid(True, alpha=0.25)
    ax.legend(frameon=False)
    finish(fig, figures_dir, "eligibility_opportunity_by_threshold")


def plot_specialization(per_dataset, figures_dir):
    summary = aggregate(per_dataset, "skill_specialization", ["group_skew", "cross_track_probability"])
    if summary.empty:
        return
    skews = sorted(summary.group_skew.unique())
    fig, axes = plt.subplots(1, len(skews), figsize=(6.2 * len(skews), 4.8), squeeze=False)
    for ax, skew in zip(axes[0], skews):
        panel = summary[summary.group_skew == skew]
        for algorithm in ALGO_ORDER:
            rows = panel[panel.algorithm == algorithm].sort_values("cross_track_probability")
            if rows.empty:
                continue
            ax.errorbar(rows.cross_track_probability, rows.mean_fairness, yerr=rows.sd_fairness.fillna(0),
                        marker="o", capsize=3, linewidth=1.7, color=PALETTE[algorithm], label=algorithm)
        ax.set_title(f"Group-size skew = {skew:g}")
        ax.set_xlabel("Cross-track skill probability")
        ax.set_ylabel("Minimum assigned utility / 100")
        ax.grid(True, alpha=0.24)
    handles, labels = axes[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=3, frameon=False, fontsize=8)
    fig.suptitle("Fairness outcomes by skill overlap and group composition", y=1.03)
    finish(fig, figures_dir, "fairness_by_skill_overlap")


def plot_tradeoff(per_dataset, figures_dir):
    rows = per_dataset.dropna(subset=["utility_vs_max_total_descriptive", "fairness"])
    if rows.empty:
        return
    fig, ax = plt.subplots(figsize=(6.8, 5.2))
    for algorithm in ALGO_ORDER:
        subset = rows[rows.algorithm == algorithm]
        if subset.empty:
            continue
        ax.scatter(subset.utility_vs_max_total_descriptive, subset.fairness, s=25, alpha=0.68,
                   color=PALETTE[algorithm], label=algorithm, edgecolors="white", linewidths=0.3)
    ax.set_xlabel("Total utility / max-total-utility allocation (descriptive; objectives differ)")
    ax.set_ylabel("Minimum assigned utility / 100")
    ax.set_title("Skill-based fairness and utility outcomes")
    ax.grid(True, alpha=0.25)
    ax.legend(frameon=False, fontsize=8)
    finish(fig, figures_dir, "fairness_utility_tradeoff")


def scaling_fit(per_dataset):
    rows = []
    for experiment, x_name in (("skill_scaling_agents", "agents"),
                              ("skill_scaling_projects", "projects")):
        subset = per_dataset[per_dataset.experiment == experiment]
        for algorithm, algo_rows in subset.groupby("algorithm"):
            means = algo_rows.groupby(x_name).runtime_ms.mean()
            # Convert both pandas containers explicitly before masking and regression.
            # Keeping an Index in this calculation can dispatch reductions differently
            # across pandas/numpy versions; ndarray operations are stable here.
            size_values = pd.to_numeric(pd.Index(means.index), errors="coerce").to_numpy(dtype=np.float64)
            runtime_values = pd.to_numeric(means, errors="coerce").to_numpy(dtype=np.float64)
            usable = np.isfinite(size_values) & np.isfinite(runtime_values) & (size_values > 0) & (runtime_values > 0)
            size_values = size_values[usable]
            runtime_values = runtime_values[usable]
            if size_values.size < 3:
                continue
            x = np.log(size_values)
            y = np.log(runtime_values)
            slope, intercept = np.polyfit(x, y, 1)
            predicted = slope * x + intercept
            residual = float(np.sum(np.square(y - predicted), dtype=np.float64))
            centered = y - np.mean(y)
            total = float(np.sum(np.square(centered), dtype=np.float64))
            rows.append({"experiment": experiment, "size_parameter": x_name, "algorithm": algorithm,
                         "empirical_exponent": slope, "r_squared": 1.0 - residual / total if total else np.nan,
                         "size_points": len(means)})
    return pd.DataFrame(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--replace-derived", action="store_true",
                        help="replace only this run's tables/figures; raw metrics are never changed")
    args = parser.parse_args()
    if not args.run_id.replace("_", "").replace("-", "").isalnum():
        parser.error("run-id may contain only letters, digits, underscores, and hyphens")
    metrics = os.path.join(ROOT, "results", "metrics", "skill_based", args.run_id)
    tables = os.path.join(ROOT, "results", "tables", "skill_based", args.run_id)
    figures = os.path.join(ROOT, "results", "figures", "skill_based", args.run_id)
    existing = [path for path in (tables, figures) if os.path.exists(path)]
    if existing and not args.replace_derived:
        parser.error("analysis outputs already exist: " + ", ".join(existing) +
                     "; rerun with --replace-derived to replace only these derived files")
    if args.replace_derived:
        derived_roots = [os.path.join(ROOT, "results", "tables", "skill_based"),
                         os.path.join(ROOT, "results", "figures", "skill_based")]
        for path, allowed_root in zip((tables, figures), derived_roots):
            resolved_root = os.path.realpath(allowed_root)
            resolved_path = os.path.realpath(path)
            if os.path.commonpath((resolved_root, resolved_path)) != resolved_root or resolved_path == resolved_root:
                parser.error("refusing to remove a path outside the run-specific derived-output folder")
            if os.path.isdir(path):
                shutil.rmtree(path)
            elif os.path.exists(path):
                os.remove(path)
    runs = load_results(metrics)
    if runs.empty:
        parser.error(f"no runs.csv files found under {metrics}")
    os.makedirs(tables)
    os.makedirs(figures)
    problems = integrity_problems(runs)
    problems.extend(check_run_manifest(runs, os.path.join(metrics, "run_manifest.json")))
    per_dataset = make_per_dataset(runs)
    save_table(per_dataset, tables, "per_dataset")
    summary = summarize(per_dataset)
    save_table(summary, tables, "summary_by_experiment_algorithm")
    fit = scaling_fit(per_dataset)
    if not fit.empty:
        save_table(fit, tables, "empirical_scaling_fit")
    for experiment, x, table_name in [
        ("skill_scaling_agents", "agents", "scaling_agents"),
        ("skill_scaling_projects", "projects", "scaling_projects"),
        ("skill_capacity", "capacity_ratio", "capacity_effect"),
        ("skill_minimum_coverage", "minimum_coverage", "coverage_threshold_effect"),
        ("skill_specialization", ["group_skew", "cross_track_probability"], "specialization_effect"),
    ]:
        effect = aggregate(per_dataset, experiment, x)
        if not effect.empty:
            save_table(effect, tables, table_name)
    plot_scalability(per_dataset, "skill_scaling_agents", "agents", "runtime_vs_students", figures)
    plot_scalability(per_dataset, "skill_scaling_projects", "projects", "runtime_vs_projects", figures)
    plot_capacity(per_dataset, figures)
    plot_threshold(per_dataset, figures)
    plot_specialization(per_dataset, figures)
    plot_tradeoff(per_dataset, figures)
    report = os.path.join(tables, "integrity_report.txt")
    with open(report, "w", encoding="utf8") as stream:
        stream.write(f"raw_rows={len(runs)}\ndatasets={runs[['experiment', 'dataset']].drop_duplicates().shape[0]}\n")
        stream.write("status=" + ("OK" if not problems else "FAILED") + "\n")
        for problem in problems:
            stream.write("problem=" + problem + "\n")
    print(f"integrity: {'OK' if not problems else 'FAILED'}; {len(runs)} raw rows")
    print(f"tables: {tables}\nfigures: {figures}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
