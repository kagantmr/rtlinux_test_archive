import sys
import numpy as np
import os
import matplotlib.pyplot as plt

def parse_cyclictest_histogram(filename):
    """
    Parses 'cyclictest -h' output format:
    000000 000000
    000001 000050
    (Latency_us Count)
    """
    latencies = []
    try:
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                parts = line.split()
                if len(parts) >= 2:
                    try:
                        latency_us = int(parts[0])
                        count = int(parts[1])
                        # Reconstruct the raw data array for statistics
                        # (e.g., if 5us happened 3 times, add [5, 5, 5])
                        if count > 0:
                            latencies.extend([latency_us] * count)
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Error parsing file: {e}")
        return np.array([])
    
    return np.array(latencies)

def main():
    if len(sys.argv) < 2:
        print("Usage: python histogram.py <latency_results.txt>")
        return
    
    filename = sys.argv[1]
    if not os.path.isfile(filename):
        print(f"Error: File '{filename}' not found. Run 'make test-os' first.")
        return

    print(f"Processing cyclictest data from {filename}...")
    data = parse_cyclictest_histogram(filename)

    if len(data) == 0:
        print("No data found. Did cyclictest run correctly?")
        return

    # Statistics
    mean_val = np.mean(data)
    max_val = np.max(data)
    std_val = np.std(data)

    print(f"--- Results ---")
    print(f"Samples: {len(data)}")
    print(f"Mean Latency: {mean_val:.2f} us")
    print(f"Max Latency:  {max_val:.2f} us")
    print(f"Jitter (Std): {std_val:.2f} us")

    # Plotting
    if not os.path.exists("plots"):
        os.makedirs("plots")

    plt.figure(figsize=(10, 6))
    # We use the raw reconstructed data to let matplotlib handle the bins nicely
    plt.hist(data, bins=range(int(max_val)+2), color='#4CAF50', edgecolor='black', alpha=0.7)
    
    plt.xlabel("Latency (microseconds)")
    plt.ylabel("Frequency")
    plt.title("System Latency Histogram (Cyclictest)")
    plt.grid(axis='y', alpha=0.5)
    
    # Add stats box
    stats_text = (f"Mean: {mean_val:.2f} us\n"
                  f"Max:  {max_val:.2f} us\n"
                  f"Std:  {std_val:.2f} us")
    
    plt.gca().text(0.95, 0.95, stats_text, fontsize=12, verticalalignment='top',
                   horizontalalignment='right', transform=plt.gca().transAxes,
                   bbox=dict(facecolor='white', alpha=0.8, edgecolor='gray'))

    output_path = os.path.join("plots", "latency_plot.png")
    plt.savefig(output_path)
    print(f"Graph saved to {output_path}")

if __name__ == "__main__":
    main()