#!/usr/bin/env bash
# -----------------------------------------------------------
# run_perf.sh — clean perf capture (no program spam)
# -----------------------------------------------------------
# Usage: ./run_perf.sh ./build/rt_matrix 10
# -----------------------------------------------------------

set -e

PROGRAM=$1
DURATION=${2:-10}
OUTDIR="perf_runs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

PROG_LOG="${OUTDIR}/program_${TIMESTAMP}.log"
PERF_LOG="${OUTDIR}/perf_${TIMESTAMP}.log"

mkdir -p "$OUTDIR"

EVENTS="task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,branches,branch-misses"

echo "[+] Running perf for ${DURATION}s on ${PROGRAM}"
echo "[+] Program log: ${PROG_LOG}"
echo "[+] Perf stats:  ${PERF_LOG}"
echo "-----------------------------------------------------------"

# Redirect stdout (program output) to program log,
# redirect perf's stderr to perf log.
sudo timeout "$DURATION" perf stat -r 3 -e $EVENTS "$PROGRAM" >"$PROG_LOG" 2>"$PERF_LOG"

echo "-----------------------------------------------------------"
echo "[✓] Perf run complete."
echo "[i] Program output saved to $PROG_LOG"
echo "[i] Perf metrics saved to  $PERF_LOG"