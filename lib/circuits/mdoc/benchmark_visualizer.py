#!/usr/bin/env python3
"""
Google Benchmark Results Visualizer
Generates performance charts from benchmark JSON output.
"""

import json
import sys
import argparse
from pathlib import Path
from typing import List, Dict, Any

try:
    import matplotlib.pyplot as plt
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
except ImportError:
    print("Error: matplotlib is required. Install with: pip3 install matplotlib")
    sys.exit(1)


def load_benchmark_results(json_file: Path) -> Dict[str, Any]:
    """Load benchmark results from JSON file."""
    with open(json_file, 'r') as f:
        return json.load(f)


def extract_benchmark_data(results: Dict[str, Any]) -> List[Dict[str, Any]]:
    """Extract relevant benchmark data."""
    benchmarks = []
    for bench in results.get('benchmarks', []):
        benchmarks.append({
            'name': bench['name'],
            'time': bench['real_time'],
            'time_unit': bench['time_unit'],
            'cpu_time': bench['cpu_time'],
            'iterations': bench['iterations']
        })
    return benchmarks


def create_bar_chart(benchmarks: List[Dict[str, Any]], output_file: Path):
    """Create a bar chart comparing benchmark times."""
    names = [b['name'].replace('BM_', '') for b in benchmarks]
    times = [b['time'] for b in benchmarks]
    time_unit = benchmarks[0]['time_unit'] if benchmarks else 'ns'
    
    plt.figure(figsize=(12, 6))
    bars = plt.bar(names, times, color=['#4285f4', '#ea4335', '#fbbc04'])
    
    # Add value labels on bars
    for bar in bars:
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.2f}',
                ha='center', va='bottom', fontsize=10)
    
    plt.xlabel('Benchmark', fontsize=12, fontweight='bold')
    plt.ylabel(f'Time ({time_unit})', fontsize=12, fontweight='bold')
    plt.title('mDoc ZK Proof Benchmark Results', fontsize=14, fontweight='bold')
    plt.xticks(rotation=45, ha='right')
    plt.grid(axis='y', alpha=0.3, linestyle='--')
    plt.tight_layout()
    
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Bar chart saved to: {output_file}")
    plt.close()


def create_comparison_chart(benchmarks: List[Dict[str, Any]], output_file: Path):
    """Create a comparison chart showing CPU time vs Real time."""
    names = [b['name'].replace('BM_', '') for b in benchmarks]
    real_times = [b['time'] for b in benchmarks]
    cpu_times = [b['cpu_time'] for b in benchmarks]
    
    x = range(len(names))
    width = 0.35
    
    plt.figure(figsize=(12, 6))
    plt.bar([i - width/2 for i in x], real_times, width, label='Real Time', color='#4285f4')
    plt.bar([i + width/2 for i in x], cpu_times, width, label='CPU Time', color='#ea4335')
    
    plt.xlabel('Benchmark', fontsize=12, fontweight='bold')
    plt.ylabel('Time (ns)', fontsize=12, fontweight='bold')
    plt.title('Real Time vs CPU Time Comparison', fontsize=14, fontweight='bold')
    plt.xticks(x, names, rotation=45, ha='right')
    plt.legend()
    plt.grid(axis='y', alpha=0.3, linestyle='--')
    plt.tight_layout()
    
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Comparison chart saved to: {output_file}")
    plt.close()


def print_summary(benchmarks: List[Dict[str, Any]]):
    """Print a text summary of benchmark results."""
    print("\n" + "="*70)
    print("BENCHMARK SUMMARY")
    print("="*70)
    
    for bench in benchmarks:
        print(f"\n{bench['name']}:")
        print(f"  Real Time:   {bench['time']:.2f} {bench['time_unit']}")
        print(f"  CPU Time:    {bench['cpu_time']:.2f} {bench['time_unit']}")
        print(f"  Iterations:  {bench['iterations']:,}")
    
    print("\n" + "="*70)


def main():
    parser = argparse.ArgumentParser(
        description='Visualize Google Benchmark results from JSON output'
    )
    parser.add_argument('json_file', type=Path, help='Path to benchmark JSON file')
    parser.add_argument('-o', '--output-dir', type=Path, default=Path('.'),
                       help='Output directory for charts (default: current directory)')
    parser.add_argument('--no-summary', action='store_true',
                       help='Skip printing text summary')
    
    args = parser.parse_args()
    
    if not args.json_file.exists():
        print(f"Error: File not found: {args.json_file}")
        sys.exit(1)
    
    # Create output directory if needed
    args.output_dir.mkdir(parents=True, exist_ok=True)
    
    # Load and process results
    print(f"Loading benchmark results from: {args.json_file}")
    results = load_benchmark_results(args.json_file)
    benchmarks = extract_benchmark_data(results)
    
    if not benchmarks:
        print("Error: No benchmark data found in JSON file")
        sys.exit(1)
    
    print(f"Found {len(benchmarks)} benchmark(s)")
    
    # Generate visualizations
    bar_chart_file = args.output_dir / 'benchmark_bar_chart.png'
    comparison_file = args.output_dir / 'benchmark_comparison.png'
    
    create_bar_chart(benchmarks, bar_chart_file)
    create_comparison_chart(benchmarks, comparison_file)
    
    # Print summary
    if not args.no_summary:
        print_summary(benchmarks)
    
    print(f"\n✓ All visualizations saved to: {args.output_dir}")


if __name__ == '__main__':
    main()
