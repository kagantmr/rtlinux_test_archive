import numpy as np
import matplotlib.pyplot as plt

data = np.loadtxt(input("Enter log file path: "))
plt.hist(data, bins=100)
plt.xlabel("Latency (ns)")
plt.ylabel("Count")
plt.title("RTLinux Merge Sort Timing Distribution")
plt.savefig("plots/rt_sort_stats.png")
print("Saved histogram")