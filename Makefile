# -------- Configuration --------
CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Wno-unused-parameter -O2 -D_GNU_SOURCE -ffast-math
LDFLAGS := -lrt -lpthread -lm 
INCLUDE := -Iinclude

# Default to rt_fft if not specified
PROG ?= rt_fft

SRC  := src/userspace/$(PROG).c
OUT  := build/userspace/$(PROG)

# -------- Rules --------
.PHONY: all clean run server test-os plot help perf

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

# 3. ANALYSIS: Run Cyclictest (Unique file per program)
# Now saves to latency_<PROGRAM_NAME>.txt
test-os:
	@echo "Running System Latency Test for context: $(PROG)..."
	sudo cyclictest -l10000 -m -S -p80 -i10000 -h900 -q > latency_$(PROG).txt
	@echo "Done. Results saved to 'latency_$(PROG).txt'."

# 4. PLOT: Generate Graph (Unique image per program)
# Passes the program name to the python script
plot:
	@echo "Generating Graph for $(PROG)..."
	python3 lib/histogram.py latency_$(PROG).txt $(PROG)

# 5. PERF: Run perf stat (Profile performance)
# Saves stats to perf_<PROGRAM_NAME>.txt
perf: $(OUT)
	@echo "Running perf stat on $(PROG)..."
	@echo "Press Ctrl+C to stop the application and generate the report."
	sudo perf stat -d -o perf_$(PROG).txt $(OUT)
	@echo "Done. Performance stats saved to 'perf_$(PROG).txt'."