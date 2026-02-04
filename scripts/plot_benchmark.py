from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "build" / "bin" / "Release" / "benchmark_results.csv"
OUT_DIR = ROOT / "charts"
OUT_DIR.mkdir(exist_ok=True)

df = pd.read_csv(CSV_PATH)

EVENTS_PER_PRODUCER = 500_000
df["TotalEvents"] = df["Producers"] * EVENTS_PER_PRODUCER

# -----------------------------
# Throughput (MEPS)
# -----------------------------
df["Mutex_MEPS"] = df["TotalEvents"] / (df["Mutex_us"] / 1_000_000)
df["Bus_MEPS"] = df["TotalEvents"] / (df["Bus_us"] / 1_000_000)

plt.rcParams.update({
    "font.size": 12,
    "axes.titlesize": 14,
    "axes.labelsize": 12,
    "legend.fontsize": 10,
    "figure.dpi": 120,
})

# -----------------------------
# Throughput plots
# -----------------------------
for p in sorted(df["Producers"].unique()):
    sub = df[df["Producers"] == p]
    fig, ax = plt.subplots(figsize=(8, 5))

    ax.plot(sub["Consumers"], sub["Mutex_MEPS"], marker="o", label="MutexQueue")
    ax.plot(sub["Consumers"], sub["Bus_MEPS"], marker="s", linestyle="--", label="EventBus")

    ax.set_title(f"Throughput vs Consumers (P={p})")
    ax.set_xlabel("Consumers")
    ax.set_ylabel("Million events / second")
    ax.grid(True, alpha=0.4)
    ax.legend()

    fig.tight_layout()
    fig.savefig(OUT_DIR / f"throughput_P{p}.png")
    plt.close(fig)

# -----------------------------
# Latency plots (AVG + P99)
# -----------------------------
for p in sorted(df["Producers"].unique()):
    sub = df[df["Producers"] == p]
    fig, ax = plt.subplots(figsize=(8, 5))

    ax.plot(sub["Consumers"], sub["MutexAvgLatency_us"],
            marker="o", label="Mutex Avg")
    ax.plot(sub["Consumers"], sub["BusAvgLatency_us"],
            marker="s", linestyle="--", label="Bus Avg")

    ax.plot(sub["Consumers"], sub["MutexP99Latency_us"],
            marker="o", linestyle=":", label="Mutex P99")
    ax.plot(sub["Consumers"], sub["BusP99Latency_us"],
            marker="s", linestyle="-.", label="Bus P99")

    ax.set_title(f"Latency vs Consumers (P={p})")
    ax.set_xlabel("Consumers")
    ax.set_ylabel("Latency (microseconds)")
    ax.grid(True, alpha=0.4)
    ax.legend()

    fig.tight_layout()
    fig.savefig(OUT_DIR / f"latency_P{p}.png")
    plt.close(fig)

print(f"Charts generated in {OUT_DIR}")
