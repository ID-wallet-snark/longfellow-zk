#!/usr/bin/env python3
"""
Advanced Benchmarks Summary Generator

Consolidates results from all advanced benchmark suites into a single report.
"""

import json
import argparse
from pathlib import Path
import sys

def load_json_results(json_file):
    """Load benchmark JSON results."""
    with open(json_file, 'r') as f:
        return json.load(f)

def analyze_multi_document(data):
    """Analyze multi-document benchmark results."""
    benchmarks = data.get('benchmarks', [])
    
    results = {}
    for bm in benchmarks:
        if 'Document' in bm['name']:
            doc_type = bm['name'].split('_')[2]  # Extract document type
            results[doc_type] = {
                'circuit_gen_ns': bm.get('circuit_gen_ns', 0),
                'prover_ns': bm.get('prover_ns', 0),
                'verifier_ns': bm.get('verifier_ns', 0),
                'total_ns': bm.get('total_ns', 0),
                'mdoc_size_bytes': bm.get('mdoc_size_bytes', 0),
                'proof_size_bytes': bm.get('proof_size_bytes', 0),
            }
    
    return results

def analyze_memory(data):
    """Analyze memory benchmark results."""
    benchmarks = data.get('benchmarks', [])
    
    results = {}
    for bm in benchmarks:
        if 'Memory' in bm['name']:
            phase = bm['name'].split('_')[2]  # Extract phase
            results[phase] = {
                'rss_delta_mb': bm.get('rss_delta_mb', 0),
                'rss_after_mb': bm.get('rss_after_mb', 0),
            }
    
    return results

def generate_summary_report(multi_doc_file, memory_file, output_file):
    """Generate consolidated summary report."""
    
    report = []
    report.append("# Advanced Benchmarks Summary Report\n")
    report.append("**Generated**: Automated Analysis\n\n")
    
    # Multi-Document Analysis
    if multi_doc_file and Path(multi_doc_file).exists():
        data = load_json_results(multi_doc_file)
        results = analyze_multi_document(data)
        
        report.append("## 1. Multi-Document Comparison\n\n")
        report.append("| Document | Circuit Gen (s) | Prover (s) | Verifier (s) | Total (s) | mDoc Size (KB) | Proof Size (KB) |\n")
        report.append("|----------|-----------------|------------|--------------|-----------|----------------|------------------|\n")
        
        for doc_type, metrics in results.items():
            report.append(f"| {doc_type} | "
                        f"{metrics['circuit_gen_ns']/1e9:.2f} | "
                        f"{metrics['prover_ns']/1e9:.2f} | "
                        f"{metrics['verifier_ns']/1e9:.2f} | "
                        f"{metrics['total_ns']/1e9:.2f} | "
                        f"{metrics['mdoc_size_bytes']/1024:.1f} | "
                        f"{metrics['proof_size_bytes']/1024:.1f} |\n")
        
        report.append("\n**Conclusion**: ")
        times = [m['total_ns'] for m in results.values()]
        if max(times) - min(times) < min(times) * 0.1:  # < 10% variation
            report.append("Document type has **minimal impact** on performance (< 10% variation).\n\n")
        else:
            report.append("Document type affects performance. Further investigation needed.\n\n")
    
    # Memory Analysis
    if memory_file and Path(memory_file).exists():
        data = load_json_results(memory_file)
        results = analyze_memory(data)
        
        report.append("## 2. Memory Profiling\n\n")
        report.append("| Phase | Peak RSS (MB) | Delta RSS (MB) |\n")
        report.append("|-------|---------------|----------------|\n")
        
        for phase, metrics in results.items():
            report.append(f"| {phase} | "
                        f"{metrics.get('rss_after_mb', 0):.1f} | "
                        f"{metrics.get('rss_delta_mb', 0):.1f} |\n")
        
        report.append("\n**Mobile Deployment**: ")
        max_rss = max([m.get('rss_after_mb', 0) for m in results.values()])
        if max_rss < 500:
            report.append(f"✅ Peak memory ({max_rss:.0f} MB) is acceptable for modern smartphones.\n\n")
        elif max_rss < 1000:
            report.append(f"⚠️ Peak memory ({max_rss:.0f} MB) may be challenging for low-end devices.\n\n")
        else:
            report.append(f"❌ Peak memory ({max_rss:.0f} MB) exceeds mobile device limits.\n\n")
    
    # Write report
    with open(output_file, 'w') as f:
        f.writelines(report)
    
    print(f"✓ Summary report saved to: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Generate advanced benchmarks summary')
    parser.add_argument('--multi-doc', help='Multi-document benchmark JSON')
    parser.add_argument('--memory', help='Memory benchmark JSON')
    parser.add_argument('-o', '--output', default='ADVANCED_BENCHMARKS_SUMMARY.md',
                       help='Output report file')
    
    args = parser.parse_args()
    
    generate_summary_report(args.multi_doc, args.memory, args.output)

if __name__ == '__main__':
    main()
