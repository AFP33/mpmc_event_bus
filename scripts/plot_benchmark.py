import pandas as pd
import matplotlib.pyplot as plt

# ---------------------------------
# Load data
# ---------------------------------
df = pd.read_csv("benchmark_results.csv")

EVENTS_PER_PRODUCER = 500_000
df["TotalEvents"] = df["Producers"] * EVENTS_PER_PRODUCER

df["Mutex_MEPS"] = df["TotalEvents"] / (df["MutexQueue_us"] / 1_000_000)
df["EventBus_MEPS"] = df["TotalEvents"] / (df["EventBus_us"] / 1_000_000)

# ---------------------------------
# Plot
# ---------------------------------
plt.figure(figsize=(10, 6))

for producers in sorted(df["Producers"].unique()):
    subset = df[df["Producers"] == producers]

    plt.plot(
        subset["Consumers"],
        subset["Mutex_MEPS"],
        marker="o",
        label=f"MutexQueue P={producers}",
    )

    plt.plot(
        subset["Consumers"],
        subset["EventBus_MEPS"],
        marker="x",
        linestyle="--",
        label=f"EventBus P={producers}",
    )

plt.xlabel("Consumers")
plt.ylabel("Throughput (Million events / second)")
plt.title("EventBus vs MutexQueue Throughput")
plt.grid(True, linestyle="--", alpha=0.5)
plt.legend()
plt.tight_layout()

plt.savefig("benchmark_graph.png", dpi=300)
plt.show()
