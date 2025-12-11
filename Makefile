# -------- Configuration --------
CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Wno-unused-parameter -O2 -D_GNU_SOURCE -ffast-math
LDFLAGS := -lrt -lpthread -lm 
INCLUDE := -Iinclude

PROG ?= rt_fft
SRC  := src/userspace/$(PROG).c
OUT  := build/userspace/$(PROG)

# -------- Rules --------
.PHONY: all clean run server test-os plot help

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p build/userspace
	@echo "Compiling $(PROG)..."
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS)

clean:
	@rm -f build/userspace/* *.txt *.png

# 1. Run the App
run: $(OUT)
	@echo "Starting $(PROG)..."
	sudo $(OUT)

# 2. Run Visualization
server:
	@echo "Starting Python Server..."
	python3 lib/tcp_server.py

# 3. ANALYSIS: Run Cyclictest (Generates latency_results.txt)
# -p80: Priority 80
# -i10000: 10ms interval (same as your FFT loop)
# -l10000: 10,000 loops
# -h400: Histogram with 400 bins (0-400us)
# -q: Quiet output (only histogram at end)
test-os:
	@echo "Running System Latency Test..."
	sudo cyclictest -l10000 -m -S -p80 -i10000 -h400 -q > latency_results.txt
	@echo "Done. Results saved to 'latency_results.txt'."

# 4. PLOT: Generate Graph from Cyclictest data
plot:
	@echo "Generating Latency Graph..."
	python3 lib/histogram.py latency_results.txt