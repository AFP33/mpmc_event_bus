from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

# -------------------------------------------------
# Paths (robust, independent of working directory)
# -------------------------------------------------
ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "build" / "bin" / "Release" / "benchmark_results.csv"
OUT_DIR = ROOT / "charts"
OUT_DIR.mkdir(exist_ok=True)

# -------------------------------------------------
# Load data
# -------------------------------------------------
df = pd.read_csv(CSV_PATH)

EVENTS_PER_PRODUCER = 500_000
df["TotalEvents"] = df["Producers"] * EVENTS_PER_PRODUCER

df["Mutex_MEPS"] = df["TotalEvents"] / (df["MutexQueue_us"] / 1_000_000)
df["EventBus_MEPS"] = df["TotalEvents"] / (df["EventBus_us"] / 1_000_000)

# -------------------------------------------------
# Matplotlib style (clean & readable)
# -------------------------------------------------
plt.rcParams.update({
    "font.size": 12,
    "axes.titlesize": 14,
    "axes.labelsize": 12,
    "legend.fontsize": 10,
    "figure.dpi": 120,
})

# -------------------------------------------------
# One chart per producer count
# -------------------------------------------------
for producers in sorted(df["Producers"].unique()):
    subset = df[df["Producers"] == producers]

    fig, ax = plt.subplots(figsize=(8, 5))

    ax.plot(
        subset["Consumers"],
        subset["Mutex_MEPS"],
        marker="o",
        linewidth=2,
        label="MutexQueue",
    )

    ax.plot(
        subset["Consumers"],
        subset["EventBus_MEPS"],
        marker="s",
        linewidth=2,
        linestyle="--",
        label="EventBus (MPMC)",
    )

    ax.set_title(f"Throughput vs Consumers (Producers = {producers})")
    ax.set_xlabel("Number of Consumers")
    ax.set_ylabel("Throughput (Million events / second)")

    ax.grid(True, linestyle="--", alpha=0.4)
    ax.legend(loc="best")

    # Optional: annotate speedup
    for _, row in subset.iterrows():
        speedup = row["EventBus_MEPS"] / row["Mutex_MEPS"]
        ax.annotate(
            f"{speedup:.2f}x",
            (row["Consumers"], row["EventBus_MEPS"]),
            textcoords="offset points",
            xytext=(0, 6),
            ha="center",
            fontsize=9,
            alpha=0.8,
        )

    fig.tight_layout()
    out_file = OUT_DIR / f"throughput_P{producers}.png"
    fig.savefig(out_file)
    plt.close(fig)

print(f"Charts generated in: {OUT_DIR}")
