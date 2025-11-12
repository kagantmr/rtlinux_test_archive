import sys
import numpy as np
import os
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) < 2:
        print("Usage: python histogram.py <test_program>")
        return
    program = sys.argv[1].strip()

    log_file = f"{program}_log.txt"
    if not os.path.isfile(log_file):
        print(f"Error: Log file '{log_file}' not found.")
        return

    print(f"Processing log file for {program}...")

    data = np.loadtxt(log_file)

    mean_val = np.mean(data)
    std_val = np.std(data)
    min_val = np.min(data)
    max_val = np.max(data)
    p99_val = np.percentile(data, 99)
    worst_jitter = max_val - min_val

    print(f"Timing Statistics for {program}:")
    print(f"Mean: {mean_val:.2f} ns")
    print(f"Standard Deviation (Jitter): {std_val:.2f} ns")
    print(f"Minimum: {min_val:.2f} ns")
    print(f"Maximum: {max_val:.2f} ns")
    print(f"99th Percentile: {p99_val:.2f} ns")
    print(f"Worst-case Jitter (Max - Min): {worst_jitter:.2f} ns")

    if not os.path.exists("plots"):
        os.makedirs("plots")

    plt.hist(data, bins=100, color='skyblue', edgecolor='black')
    plt.xlabel("Latency (ns)")
    plt.ylabel("Count")
    plt.title(f"{program} Timing Distribution")

    plt.axvline(mean_val, color='red', linestyle='dashed', linewidth=1.5, label=f"Mean: {mean_val:.2f} ns")
    plt.axvline(min_val, color='green', linestyle='dashed', linewidth=1.5, label=f"Min: {min_val:.2f} ns")
    plt.axvline(max_val, color='orange', linestyle='dashed', linewidth=1.5, label=f"Max: {max_val:.2f} ns")

    plt.legend()

    stats_text = (f"Mean: {mean_val:.2f} ns\n"
                  f"Std Dev (Jitter): {std_val:.2f} ns\n"
                  f"Min: {min_val:.2f} ns\n"
                  f"Max: {max_val:.2f} ns\n"
                  f"99th Percentile: {p99_val:.2f} ns\n"
                  f"Worst Jitter: {worst_jitter:.2f} ns")

    plt.gca().text(0.95, 0.95, stats_text, fontsize=9, verticalalignment='top',
                   horizontalalignment='right', transform=plt.gca().transAxes,
                   bbox=dict(facecolor='white', alpha=0.8, edgecolor='gray'))

    output_path = os.path.join("plots", f"{program}_hist.png")
    plt.savefig(output_path)
    print(f"Histogram saved to {output_path}")

if __name__ == "__main__":
    main()