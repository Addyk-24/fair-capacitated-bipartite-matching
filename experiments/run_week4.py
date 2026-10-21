"""Reproduce all Week 4 evidence with one command.

    python experiments/run_week4.py --suite build/experiment_suite

Runs every experiments/configs/week4_*.cfg through experiment_suite (raw rows go to
results/metrics/<name>/...), then builds the processed tables and figures with
notebooks/analyze_results.py. Existing raw results are never replaced silently:
pass --overwrite to rerun an experiment that already has results.
"""
import argparse
import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def experiment_name(cfg_path):
    with open(cfg_path, encoding="utf8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line.startswith("name") and "=" in line:
                return line.split("=", 1)[1].strip()
    raise ValueError(f"{cfg_path} has no 'name = ...' line")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--suite", required=True, help="path to the built experiment_suite executable")
    ap.add_argument("--out", default=os.path.join(ROOT, "results", "metrics"), help="raw results root")
    ap.add_argument("--overwrite", action="store_true", help="replace existing raw results")
    ap.add_argument("--skip-run", action="store_true", help="only (re)build tables and figures")
    args = ap.parse_args()

    configs = sorted(glob.glob(os.path.join(ROOT, "experiments", "configs", "week4_*.cfg")))
    names = [experiment_name(c) for c in configs]

    if not args.skip_run:
        for cfg, name in zip(configs, names):
            print(f"\n=== {name} ({os.path.basename(cfg)}) ===", flush=True)
            cmd = [args.suite, "--config", cfg, "--out", args.out]
            if args.overwrite:
                cmd.append("--overwrite")
            code = subprocess.call(cmd)
            if code != 0:
                print(f"experiment_suite failed for {name} (exit {code})", file=sys.stderr)
                return code

    print("\n=== analysis ===", flush=True)
    return subprocess.call([
        sys.executable, os.path.join(ROOT, "notebooks", "analyze_results.py"),
        "--metrics", args.out,
        "--experiments", ",".join(names),
        "--tables", os.path.join(ROOT, "results", "tables"),
        "--figures", os.path.join(ROOT, "results", "figures"),
    ])


if __name__ == "__main__":
    sys.exit(main())
