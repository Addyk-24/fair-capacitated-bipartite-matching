"""Turn raw experiment results into processed tables and figures.

Raw files (results/metrics/<experiment>/<size>/<tag>/runs.csv + params.txt) are only
read, never modified. Everything derived goes to separate folders:

    results/tables/   per_dataset.csv, summary_by_algorithm.*, capacity_effect.*, ...
    results/figures/  runtime_vs_agents.png, utility_by_capacity.png, ...

Usage:
    python notebooks/analyze_results.py \
        --experiments week4_scaling,week4_scaling_resources,week4_capacity,week4_utility_density,week4_groups

Needs only pandas, numpy and matplotlib.
"""
import argparse
import glob
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

# Algorithm used as the reference for ratios:
#  - utility is compared with the best utility achievable under the real capacities,
#  - assigned count is compared with the largest achievable count.
UTILITY_REFERENCE = "capacitated_flow"
COUNT_REFERENCE = "capacitated_flow_maxcard"

ALGO_ORDER = [
    "greedy_reference",
    "cardinality",
    "weighted",
    "capacitated_flow",
    "capacitated_flow_maxcard",
    "fairness",
]
ALGO_COLOR = {
    "greedy_reference": "#9aa0a6",
    "cardinality": "#8e44ad",
    "weighted": "#e67e22",
    "capacitated_flow": "#1f77b4",
    "capacitated_flow_maxcard": "#2ca02c",
    "fairness": "#d62728",
}

PARAM_KEYS = [
    "density",
    "popularity_skew",
    "utility_dist",
    "capacity_mode",
    "fixed_capacity",
    "groups",
    "group_skew",
    "seed",
]


# --------------------------------------------------------------------------- loading
def read_params(path):
    params = {}
    with open(path, encoding="utf8") as f:
        for line in f:
            if "=" in line:
                k, v = line.strip().split("=", 1)
                params[k] = v
    return params


def load_runs(metrics_root, experiments):
    """All raw rows of the given experiments, joined with their dataset parameters."""
    frames = []
    for exp in experiments:
        pattern = os.path.join(metrics_root, exp, "*", "*", "runs.csv")
        files = sorted(glob.glob(pattern))
        if not files:
            print(f"[skip] no results for experiment '{exp}' under {metrics_root}")
            continue
        for path in files:
            df = pd.read_csv(path, dtype={"instance_hash": str})
            params = read_params(os.path.join(os.path.dirname(path), "params.txt"))
            df["experiment"] = exp
            for key in PARAM_KEYS:
                df[key] = params.get(key)
            frames.append(df)
    if not frames:
        return pd.DataFrame()
    runs = pd.concat(frames, ignore_index=True)
    for col in ("density", "popularity_skew", "group_skew"):
        runs[col] = runs[col].astype(float)
    for col in ("fixed_capacity", "groups", "seed"):
        runs[col] = runs[col].astype(int)
    runs["fairness_score"] = pd.to_numeric(runs["fairness_score"], errors="coerce")
    # Human-readable capacity regime: fixed1, fixed2, ..., low, balanced, high.
    runs["capacity"] = np.where(
        runs["capacity_mode"] == "fixed",
        "fixed" + runs["fixed_capacity"].astype(str),
        runs["capacity_mode"],
    )
    return runs


# ----------------------------------------------------------------------- integrity
def integrity_report(runs):
    """Prints and returns problems that would make the comparison unfair."""
    problems = []
    invalid = runs[runs["valid"] != 1]
    if len(invalid):
        problems.append(f"{len(invalid)} invalid run(s): " + ", ".join(sorted(invalid["algorithm"].unique())))

    # Same dataset => every algorithm must have received the identical instance.
    hashes = runs.groupby(["experiment", "dataset"])["instance_hash"].nunique()
    bad = hashes[hashes > 1]
    if len(bad):
        problems.append(f"{len(bad)} dataset(s) where algorithms saw different instances")

    # Deterministic algorithms must give identical results on every repeat.
    spread = runs.groupby(["experiment", "dataset", "algorithm"])[["assigned_count", "total_utility"]].nunique()
    unstable = spread[(spread["assigned_count"] > 1) | (spread["total_utility"] > 1)]
    if len(unstable):
        problems.append(f"{len(unstable)} (dataset, algorithm) pair(s) with results that differ between repeats")

    if problems:
        for p in problems:
            print("[integrity] WARNING:", p)
    else:
        print(f"[integrity] OK: {len(runs)} rows, all valid, identical instances per dataset, "
              "results stable across repeats")
    return problems


# ---------------------------------------------------------------------- aggregation
def per_dataset_table(runs):
    """One row per (dataset, algorithm): repeats collapsed, runtime = median over repeats."""
    runs = runs[runs["valid"] == 1]
    keys = ["experiment", "dataset", "agents", "resources", "edges", "algorithm"] + PARAM_KEYS + ["capacity"]
    grouped = runs.groupby(keys, dropna=False)
    table = grouped.agg(
        assigned=("assigned_count", "first"),
        utility=("total_utility", "first"),
        fairness=("fairness_score", "first"),
        runtime_ms=("runtime_ms", "median"),
        repeats=("repeat", "count"),
    ).reset_index()

    # Keep ratios only where both algorithms solve the same optimization problem.
    # Utility: capacity-aware utility optimizers (plus greedy as a heuristic) are
    # compared to capacitated_flow. weighted joins only when every capacity is 1.
    # Count: compare feasible coverage heuristics and fairness/max-cardinality to
    # the capacity-aware max-cardinality optimum; capacity-1 cardinality joins only
    # on fixed-capacity-one instances.
    ref_u = (table[table["algorithm"] == UTILITY_REFERENCE][["experiment", "dataset", "utility"]]
             .rename(columns={"utility": "ref_utility"}))
    ref_n = (table[table["algorithm"] == COUNT_REFERENCE][["experiment", "dataset", "assigned"]]
             .rename(columns={"assigned": "ref_assigned"}))
    table = table.merge(ref_u, on=["experiment", "dataset"], how="left")
    table = table.merge(ref_n, on=["experiment", "dataset"], how="left")
    table["utility_efficiency"] = table["utility"] / table["ref_utility"]
    table["utility_ratio"] = table["utility_efficiency"]
    same_capacity_one = (table["capacity_mode"] == "fixed") & (table["fixed_capacity"] == 1)
    table.loc[~table["algorithm"].isin(["greedy_reference", "capacitated_flow"])
              & ~((table["algorithm"] == "weighted") & same_capacity_one), "utility_ratio"] = np.nan
    table["assigned_ratio"] = table["assigned"] / table["ref_assigned"]
    table.loc[(table["algorithm"] == "cardinality") & ~same_capacity_one, "assigned_ratio"] = np.nan
    table.loc[table["algorithm"] == "weighted", "assigned_ratio"] = np.nan
    return table.drop(columns=["ref_utility", "ref_assigned"])


def algo_sorted(names):
    return sorted(names, key=lambda a: ALGO_ORDER.index(a) if a in ALGO_ORDER else len(ALGO_ORDER))


def summary_by_algorithm(table):
    rows = []
    for algo, g in table.groupby("algorithm"):
        rows.append({
            "algorithm": algo,
            "datasets": len(g),  # one row per (experiment, dataset) for this algorithm
            "mean_utility_ratio": g["utility_ratio"].mean(),
            "min_utility_ratio": g["utility_ratio"].min(),
            "mean_assigned_ratio": g["assigned_ratio"].mean(),
            "median_runtime_ms": g["runtime_ms"].median(),
            "max_runtime_ms": g["runtime_ms"].max(),
        })
    out = pd.DataFrame(rows)
    out["_o"] = out["algorithm"].map(lambda a: ALGO_ORDER.index(a) if a in ALGO_ORDER else 99)
    return out.sort_values("_o").drop(columns="_o").reset_index(drop=True)


def mean_over_seeds(table, by):
    """Mean and std over seeds for each combination of `by` and algorithm."""
    g = table.groupby(by + ["algorithm"], dropna=False)
    out = g.agg(
        utility=("utility", "mean"),
        assigned=("assigned", "mean"),
        utility_ratio=("utility_ratio", "mean"),
        assigned_ratio=("assigned_ratio", "mean"),
        runtime_ms=("runtime_ms", "mean"),
        runtime_std=("runtime_ms", "std"),
        n=("seed", "count"),
    ).reset_index()
    return out


def complexity_fit(table, experiment, variable):
    """Empirical growth exponent: slope of log10(runtime) against log10(variable)."""
    sub = table[table["experiment"] == experiment]
    if sub.empty:
        return []
    rows = []
    for algo, g in sub.groupby("algorithm"):
        m = g.groupby(variable)["runtime_ms"].mean().reset_index()
        m = m[m["runtime_ms"] > 0]
        if len(m) < 3:
            continue
        x, y = np.log10(m[variable].astype(float)), np.log10(m["runtime_ms"])
        slope, intercept = np.polyfit(x, y, 1)
        pred = slope * x + intercept
        ss_res = float(((y - pred) ** 2).sum())
        ss_tot = float(((y - y.mean()) ** 2).sum())
        rows.append({
            "experiment": experiment,
            "variable": variable,
            "algorithm": algo,
            "exponent": slope,
            "r_squared": 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan"),
            "points": len(m),
        })
    return rows


# ------------------------------------------------------------------------- output
def to_markdown(df, floatfmt=3):
    def fmt(v):
        if isinstance(v, float):
            return "" if np.isnan(v) else f"{v:.{floatfmt}f}"
        return str(v)

    cols = list(df.columns)
    lines = ["| " + " | ".join(cols) + " |", "| " + " | ".join("---" for _ in cols) + " |"]
    for _, row in df.iterrows():
        lines.append("| " + " | ".join(fmt(row[c]) for c in cols) + " |")
    return "\n".join(lines)


def save_table(df, tables_dir, name, md=True):
    os.makedirs(tables_dir, exist_ok=True)
    df.to_csv(os.path.join(tables_dir, name + ".csv"), index=False)
    if md:
        with open(os.path.join(tables_dir, name + ".md"), "w", encoding="utf8") as f:
            f.write(to_markdown(df) + "\n")
    print(f"[table] {name}")


def finish_figure(fig, figs_dir, name):
    os.makedirs(figs_dir, exist_ok=True)
    fig.tight_layout()
    fig.savefig(os.path.join(figs_dir, name + ".png"), dpi=150)
    plt.close(fig)
    print(f"[figure] {name}.png")


# --------------------------------------------------------------------------- plots
def plot_runtime(table, experiment, variable, label, figs_dir, name):
    sub = table[table["experiment"] == experiment]
    if sub.empty:
        print(f"[skip] {name}: no data for {experiment}")
        return
    m = mean_over_seeds(sub, [variable])
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for algo in algo_sorted(m["algorithm"].unique()):
        g = m[m["algorithm"] == algo].sort_values(variable)
        ax.errorbar(g[variable], g["runtime_ms"], yerr=g["runtime_std"].fillna(0), marker="o",
                    capsize=3, color=ALGO_COLOR.get(algo), label=algo)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel(label)
    ax.set_ylabel("runtime (ms, median of repeats, mean over seeds)")
    ax.set_title(f"Runtime vs {label}")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    finish_figure(fig, figs_dir, name)


def grouped_bars(table, experiment, category, value, ylabel, title, figs_dir, name, order=None, hline=None):
    sub = table[table["experiment"] == experiment]
    if sub.empty:
        print(f"[skip] {name}: no data for {experiment}")
        return
    m = mean_over_seeds(sub, [category])
    cats = order if order else sorted(m[category].unique(), key=str)
    cats = [c for c in cats if c in set(m[category])]
    algos = algo_sorted(m["algorithm"].unique())
    width = 0.8 / max(len(algos), 1)
    fig, ax = plt.subplots(figsize=(max(7, 1.3 * len(cats) + 3), 4.5))
    for i, algo in enumerate(algos):
        vals = []
        for c in cats:
            row = m[(m[category] == c) & (m["algorithm"] == algo)]
            vals.append(float(row[value].iloc[0]) if len(row) else np.nan)
        ax.bar(np.arange(len(cats)) + i * width, vals, width, color=ALGO_COLOR.get(algo), label=algo)
    ax.set_xticks(np.arange(len(cats)) + width * (len(algos) - 1) / 2)
    ax.set_xticklabels([str(c) for c in cats])
    ax.set_xlabel(category.replace("_", " "))
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    if hline is not None:
        ax.axhline(hline, color="black", linewidth=0.8, linestyle="--")
        ax.set_ylim(0, hline * 1.22)  # headroom so the legend never covers a bar
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend(fontsize=8, loc="upper center", ncol=len(algos))
    finish_figure(fig, figs_dir, name)


def plot_fairness_tradeoff(table, figs_dir):
    have = table.dropna(subset=["fairness", "utility_efficiency"])
    if have.empty:
        print("[skip] fairness_vs_utility: no fairness_score in the results yet "
              "(no fairness metric is wired into the runner)")
        return False
    fig, ax = plt.subplots(figsize=(6.5, 5))
    for algo in algo_sorted(have["algorithm"].unique()):
        g = have[have["algorithm"] == algo]
        ax.scatter(g["utility_efficiency"], g["fairness"], s=18, alpha=0.7, color=ALGO_COLOR.get(algo), label=algo)
    ax.set_xlabel("realized utility / max-total-utility allocation (descriptive)")
    ax.set_ylabel("minimum assigned utility / 100")
    ax.set_title("Fairness–utility outcome trade-off")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    finish_figure(fig, figs_dir, "fairness_vs_utility")
    return True


# ----------------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--metrics", default="results/metrics", help="root of the raw results")
    ap.add_argument("--experiments", required=True, help="comma-separated experiment names")
    ap.add_argument("--tables", default="results/tables")
    ap.add_argument("--figures", default="results/figures")
    args = ap.parse_args(argv)

    experiments = [e.strip() for e in args.experiments.split(",") if e.strip()]
    runs = load_runs(args.metrics, experiments)
    if runs.empty:
        print("error: no raw results found; run the experiments first", file=sys.stderr)
        return 1

    problems = integrity_report(runs)
    table = per_dataset_table(runs)
    save_table(table, args.tables, "per_dataset", md=False)

    summary = summary_by_algorithm(table)
    save_table(summary, args.tables, "summary_by_algorithm")
    fairness = table.dropna(subset=["fairness"]).groupby("algorithm").agg(
        datasets=("fairness", "count"), mean_min_normalized_utility=("fairness", "mean"),
        min_min_normalized_utility=("fairness", "min"), mean_total_utility=("utility", "mean"),
        mean_assigned=("assigned", "mean"),
    ).reset_index()
    if not fairness.empty:
        save_table(fairness, args.tables, "fairness_summary")

    fits = complexity_fit(table, "week4_scaling", "agents") + complexity_fit(
        table, "week4_scaling_resources", "resources")
    if fits:
        save_table(pd.DataFrame(fits), args.tables, "complexity_fit")

    cap = table[table["experiment"] == "week4_capacity"]
    if not cap.empty:
        cap_order = [c for c in ["fixed1", "fixed2", "fixed4", "low", "balanced", "high"] if c in set(cap["capacity"])]
        effect = mean_over_seeds(cap, ["capacity"])
        effect["_o"] = effect["capacity"].map(lambda c: cap_order.index(c))
        effect = effect.sort_values(["_o", "algorithm"]).drop(columns="_o")
        save_table(effect[["capacity", "algorithm", "assigned", "utility", "utility_ratio", "assigned_ratio",
                           "runtime_ms", "n"]], args.tables, "capacity_effect")
    else:
        cap_order = None

    dist = table[table["experiment"] == "week4_utility_density"]
    if not dist.empty:
        save_table(mean_over_seeds(dist, ["utility_dist", "density"])[
            ["utility_dist", "density", "algorithm", "assigned", "utility", "utility_ratio", "runtime_ms", "n"]],
            args.tables, "utility_density_effect")

    plot_runtime(table, "week4_scaling", "agents", "number of agents", args.figures, "runtime_vs_agents")
    plot_runtime(table, "week4_scaling_resources", "resources", "number of resources", args.figures,
                 "runtime_vs_resources")
    grouped_bars(table, "week4_capacity", "capacity", "utility_ratio", "utility / best achievable utility",
                 "Utility by capacity regime", args.figures, "utility_by_capacity", order=cap_order, hline=1.0)
    grouped_bars(table, "week4_capacity", "capacity", "assigned_ratio", "assigned / max achievable assigned",
                 "Assigned agents by capacity regime", args.figures, "assigned_by_capacity", order=cap_order,
                 hline=1.0)
    grouped_bars(table, "week4_utility_density", "utility_dist", "utility_ratio",
                 "utility / best achievable utility", "Utility by utility distribution", args.figures,
                 "utility_by_distribution", order=["uniform", "bell", "low", "high"], hline=1.0)
    plot_fairness_tradeoff(table, args.figures)

    print("\n" + to_markdown(summary))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
