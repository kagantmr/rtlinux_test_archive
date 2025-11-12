import subprocess
import csv
import re
import sys
import matplotlib.pyplot as plt
from datetime import datetime

# ---------------- CONFIG ----------------
METRICS = [
    "task-clock",
    "context-switches",
    "cpu-migrations",
    "page-faults",
    "cycles",
    "instructions",
    "branches",
    "branch-misses"
]
# ----------------------------------------

def run_perf(program_path: str, duration: int = 10, runs: int = 3):
    cmd = [
        "sudo", "timeout", str(duration),
        "perf", "stat",
        "-e", ",".join(METRICS),
        "-r", str(runs),
        program_path
    ]

    print(f"[+] Running: {' '.join(cmd)}\n")
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stderr  # perf writes stats to stderr


def parse_perf_output(output: str):
    data = {}
    for line in output.splitlines():
        for metric in METRICS:
            if metric in line:
                # Extract the numeric value (handles commas etc.)
                match = re.search(r"([\d,]+)", line)
                if match:
                    value = match.group(1).replace(",", "")
                    data[metric] = int(value)
    return data


def save_to_csv(data: dict, filename: str):
    with open(filename, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["metric", "value"])
        for k, v in data.items():
            writer.writerow([k, v])
    print(f"[+] Saved CSV to {filename}")


def plot_metrics(data: dict, out_path: str):
    plt.figure(figsize=(10, 6))
    metrics = list(data.keys())
    values = list(data.values())

    plt.bar(metrics, values, color="steelblue")
    plt.title("RTLinux Performance Metrics")
    plt.ylabel("Value")
    plt.xticks(rotation=45, ha="right")
    plt.grid(axis="y", linestyle="--", alpha=0.7)
    plt.tight_layout()
    plt.savefig(out_path)
    print(f"[+] Plot saved to {out_path}")


def main():
    if len(sys.argv) < 2:
        print("Usage: sudo python3 rt_perf_test.py <program_path> [duration_seconds]")
        sys.exit(1)

    program = sys.argv[1]
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 10

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = f"perf_results_{timestamp}"

    subprocess.run(["mkdir", "-p", output_dir])

    print(f"[*] Running perf test for {program} ({duration}s)...\n")
    raw_output = run_perf(program, duration)
    data = parse_perf_output(raw_output)

    csv_path = f"{output_dir}/perf_summary.csv"
    plot_path = f"{output_dir}/perf_plot.png"

    save_to_csv(data, csv_path)
    plot_metrics(data, plot_path)

    print("\n[✓] Done.")


if __name__ == "__main__":
    main()