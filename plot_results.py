#!/usr/bin/env python3
"""
Plot fractal-flame benchmark results.

Usage:
    python plot_results.py results/results_20260727_204003.csv

Produces, next to the CSV:
    <csv>_throughput.png   iterations/sec vs thread count (occupancy curve)
    <csv>_speedup.png      speedup over serial vs thread count

Written to survive new engines: any engine that isn't "Serial" is treated
as a parallel engine and plotted automatically, so uncommenting OpenMP in
the benchmark and re-running Just Works -- OpenMP shows up as its own curve.
"""

import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt

# scale iters/sec to millions for readable axes
PER_SEC_SCALE = 1e6
PER_SEC_LABEL = "Iterations/sec (millions)"


def load(csv_path):
    # skip_blank_lines handles the trailing newline the benchmark writes
    df = pd.read_csv(csv_path, skip_blank_lines=True)
    df = df.dropna(how="all")
    return df


def throughput_plot(df, out_path):
    """iters/sec vs threads, one line per (engine, seed), log-x."""
    parallel = df[df["engine"] != "Serial"]

    fig, ax = plt.subplots(figsize=(8, 5))
    for engine in sorted(parallel["engine"].unique()):
        eng_df = parallel[parallel["engine"] == engine]
        for seed in sorted(eng_df["seed"].unique()):
            s = eng_df[eng_df["seed"] == seed].sort_values("threads")
            ax.plot(
                s["threads"],
                s["iter_per_sec"] / PER_SEC_SCALE,
                marker="o",
                markersize=4,
                alpha=0.35,
                linewidth=1,
                label=None,  # per-seed lines are context, not legend clutter
            )
        # overlay the per-engine mean across seeds as the bold line
        mean = (
            eng_df.groupby("threads")["iter_per_sec"].mean().sort_index()
        )
        ax.plot(
            mean.index,
            mean.values / PER_SEC_SCALE,
            marker="o",
            linewidth=2.5,
            label=f"{engine} (mean of {eng_df['seed'].nunique()} seeds)",
        )

    ax.set_xscale("log", base=2)
    ax.set_xlabel("Threads")
    ax.set_ylabel(PER_SEC_LABEL)
    ax.set_title("Throughput vs thread count (occupancy scaling)")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")


def speedup_plot(df, out_path):
    """Speedup over that seed's serial baseline, vs threads."""
    serial = df[df["engine"] == "Serial"]
    # seed -> serial iters/sec, so each seed is normalized to its own baseline
    baseline = serial.set_index("seed")["iter_per_sec"].to_dict()

    parallel = df[df["engine"] != "Serial"].copy()
    parallel["speedup"] = parallel.apply(
        lambda row: row["iter_per_sec"] / baseline[row["seed"]], axis=1
    )

    fig, ax = plt.subplots(figsize=(8, 5))
    for engine in sorted(parallel["engine"].unique()):
        eng_df = parallel[parallel["engine"] == engine]
        mean = eng_df.groupby("threads")["speedup"].mean().sort_index()
        ax.plot(
            mean.index,
            mean.values,
            marker="o",
            linewidth=2.5,
            label=f"{engine}",
        )

    ax.set_xscale("log", base=2)
    ax.axhline(1.0, color="gray", linestyle="--", linewidth=1, label="serial (1x)")
    ax.set_xlabel("Threads")
    ax.set_ylabel("Speedup over serial")
    ax.set_title("Speedup vs serial baseline")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")


def main():
    if len(sys.argv) != 2:
        print("usage: python plot_results.py <results.csv>")
        sys.exit(1)

    csv_path = Path(sys.argv[1])
    df = load(csv_path)

    stem = csv_path.with_suffix("")  # drop .csv
    throughput_plot(df, f"{stem}_throughput.png")
    speedup_plot(df, f"{stem}_speedup.png")


if __name__ == "__main__":
    main()
