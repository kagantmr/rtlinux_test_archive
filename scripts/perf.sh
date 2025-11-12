#!/usr/bin/env bash
# -----------------------------------------------------------
# perf_corebind.sh — run perf on a single isolated core
# -----------------------------------------------------------
# Usage:
#   ./perf_corebind.sh ./build/rt_matrix [core_id] [duration_seconds]
# -----------------------------------------------------------

set -e

PROGRAM=$1
CORE=${2:-2}           # default: CPU core 2
DURATION=${3:-10}      # default: 10 seconds
OUTDIR="results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

mkdir -p "$OUTDIR"

PERF_LOG="${OUTDIR}/perf_${TIMESTAMP}.log"
PROG_LOG="${OUTDIR}/program_${TIMESTAMP}.log"

EVENTS="task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,branches,branch-misses"

if [ -z "$PROGRAM" ]; then
    echo "Usage: $0 <program_path> [core_id] [duration_seconds]"
    exit 1
fi

echo "[+] Running $PROGRAM on core $CORE for ${DURATION}s"
echo "[+] Perf output:   $PERF_LOG"
echo "[+] Program output: $PROG_LOG"
echo "-----------------------------------------------------------"

# Check CPU existence
if [ ! -d "/sys/devices/system/cpu/cpu${CORE}" ]; then
    echo "[!] CPU ${CORE} does not exist on this system."
    exit 1
fi

# Lock CPU to performance governor (optional but ideal)
if [ -f "/sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_governor" ]; then
    echo performance | sudo tee /sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_governor >/dev/null
    echo "[+] Set CPU${CORE} to performance mode"
else
    echo "[!] CPU frequency scaling not supported (likely VM)"
fi

# Run perf on that CPU using taskset
sudo taskset -c "$CORE" perf stat -e $EVENTS -r 3 timeout "$DURATION" "$PROGRAM" >"$PROG_LOG" 2>"$PERF_LOG"

echo "-----------------------------------------------------------"
echo "[✓] Completed."
echo "[i] Program output → $PROG_LOG"
echo "[i] Perf metrics   → $PERF_LOG"