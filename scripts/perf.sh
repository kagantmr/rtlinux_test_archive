#!/usr/bin/env bash
# -----------------------------------------------------------
# run_perf.sh — run perf stat on a real-time program
# -----------------------------------------------------------
# Usage:
#   ./run_perf.sh ./build/rt_matrix 10
# -----------------------------------------------------------

set -e  # stop on errors

PROGRAM=$1
DURATION=${2:-10}  # default 10 seconds
OUTDIR="perf_runs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOGFILE="${OUTDIR}/perf_${TIMESTAMP}.log"

# Create directory if needed
mkdir -p "$OUTDIR"

# Perf events we actually care about
EVENTS="task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,branches,branch-misses"

echo "[+] Running perf for ${DURATION}s on ${PROGRAM}"
echo "[+] Output will be saved to ${LOGFILE}"
echo "-----------------------------------------------------------"

# Run perf 3 times with timeout
sudo timeout "$DURATION" perf stat -r 3 -e $EVENTS "$PROGRAM" 2>&1 | tee "$LOGFILE"

echo "-----------------------------------------------------------"
echo "[✓] Perf run complete."
echo "[i] Saved raw output to $LOGFILE"