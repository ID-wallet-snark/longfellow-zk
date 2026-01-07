#!/bin/bash
# Automated Benchmark Runner with Visualization
# Runs mdoc benchmarks and generates performance visualizations

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../../build"
BENCHMARK_BINARY="${BUILD_DIR}/lib/circuits/mdoc/mdoc_benchmark"
RESULTS_DIR="${SCRIPT_DIR}/benchmark_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  mDoc ZK Proof Benchmark Runner${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Check if benchmark binary exists
if [ ! -f "$BENCHMARK_BINARY" ]; then
    echo -e "${YELLOW}Benchmark binary not found. Building...${NC}"
    cd "$BUILD_DIR"
    cmake --build . --target mdoc_benchmark
    cd "$SCRIPT_DIR"
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

# Run benchmarks with different output formats
echo -e "${GREEN}Running benchmarks...${NC}"

# Console output
echo -e "\n${BLUE}Console Output:${NC}"
"$BENCHMARK_BINARY" --benchmark_repetitions=3

# JSON output
JSON_FILE="${RESULTS_DIR}/benchmark_${TIMESTAMP}.json"
echo -e "\n${GREEN}Saving JSON results to: ${JSON_FILE}${NC}"
"$BENCHMARK_BINARY" --benchmark_format=json --benchmark_out="$JSON_FILE"

# CSV output
CSV_FILE="${RESULTS_DIR}/benchmark_${TIMESTAMP}.csv"
echo -e "${GREEN}Saving CSV results to: ${CSV_FILE}${NC}"
"$BENCHMARK_BINARY" --benchmark_format=csv --benchmark_out="$CSV_FILE"

# Generate visualizations
echo -e "\n${BLUE}Generating visualizations...${NC}"
CHART_DIR="${RESULTS_DIR}/charts_${TIMESTAMP}"
mkdir -p "$CHART_DIR"

if command -v python3 &> /dev/null; then
    # Check if matplotlib is installed
    if python3 -c "import matplotlib" 2>/dev/null; then
        python3 "${SCRIPT_DIR}/benchmark_visualizer.py" "$JSON_FILE" -o "$CHART_DIR"
    else
        echo -e "${YELLOW}Warning: matplotlib not installed. Install with: pip3 install matplotlib${NC}"
        echo -e "${YELLOW}Skipping visualization generation.${NC}"
    fi
else
    echo -e "${YELLOW}Warning: python3 not found. Skipping visualization generation.${NC}"
fi

# Create symlink to latest results
ln -sf "benchmark_${TIMESTAMP}.json" "${RESULTS_DIR}/latest.json"
ln -sf "charts_${TIMESTAMP}" "${RESULTS_DIR}/latest_charts"

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}  Benchmark Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Results saved to: ${RESULTS_DIR}"
echo -e "Latest JSON: ${RESULTS_DIR}/latest.json"
echo -e "Latest charts: ${RESULTS_DIR}/latest_charts"
echo -e ""
