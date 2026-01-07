#!/usr/bin/env python3
"""
UX Analyzer for mDoc ZK Wallet

Analyzes user experience metrics: latency, battery, network bandwidth
"""

import json
import argparse
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

plt.style.use('seaborn-v0_8-darkgrid')

def analyze_latency(results_file):
    """Analyze end-to-end latency from benchmark results."""
    with open(results_file, 'r') as f:
        data = json.load(f)
    
    benchmarks = data.get('benchmarks', [])
    
    # Extract timing data
    latency = {
        'circuit_gen': 0,
        'prover': 0,
        'verifier': 0,
        'total': 0
    }
    
    for bm in benchmarks:
        if '1Attr' in bm['name'] or 'Scalability_1' in bm['name']:
            latency['circuit_gen'] = bm.get('circuit_gen_ns', 0) / 1e9
            latency['prover'] = bm.get('prover_ns', 0) / 1e9
            latency['verifier'] = bm.get('verifier_ns', 0) / 1e9
            latency['total'] = bm.get('total_ns', 0) / 1e9
            break
    
    # Add realistic overhead
    latency['decompression'] = 0.5  # zstd decompression
    latency['ui_overhead'] = 0.2  # UI rendering
    latency['total_ux'] = sum(latency.values())
    
    return latency

def estimate_battery(latency):
    """Estimate battery consumption based on timing."""
    # Assumptions:
    # - CPU TDP: ~5W for smartphone SoC under load
    # - Memory: ~0.5W
    # - Total power: ~5.5W
    
    power_w = 5.5
    time_h = latency['total'] / 3600  # Convert seconds to hours
    energy_wh = power_w * time_h
    
    # Convert to mAh (assuming 3.7V battery)
    voltage = 3.7
    capacity_mah = (energy_wh / voltage) * 1000
    
    # Smartphone battery capacities
    batteries = {
        'iPhone 15': 3349,  # mAh
        'Samsung S24': 4000,
        'Pixel 8': 4575,
        'Budget phone': 3000
    }
    
    impact = {}
    for phone, battery_mah in batteries.items():
        percentage = (capacity_mah / battery_mah) * 100
        proofs_per_charge = battery_mah / capacity_mah
        impact[phone] = {
            'battery_mah': battery_mah,
            'consumption_pct': percentage,
            'proofs_per_charge': proofs_per_charge
        }
    
    return capacity_mah, impact

def analyze_network(proof_size_kb=320, circuit_size_kb=278):
    """Analyze network bandwidth impact."""
    # Data transfer
    download_kb = circuit_size_kb  # Download circuit once
    upload_kb = proof_size_kb  # Upload proof
    
    # Network speeds (Kbps)
    networks = {
        '3G': 384,
        '4G': 10000,
        '5G': 50000,
        'WiFi': 100000
    }
    
    impact = {}
    for network, speed_kbps in networks.items():
        download_time = (download_kb * 8) / speed_kbps  # seconds
        upload_time = (upload_kb * 8) / speed_kbps
        total_time = download_time + upload_time
        
        impact[network] = {
            'download_time_s': download_time,
            'upload_time_s': upload_time,
            'total_time_s': total_time
        }
    
    return impact

def create_latency_waterfall(latency, output_dir):
    """Create waterfall chart for latency breakdown."""
    phases = ['Decompression', 'Circuit Gen', 'Prover', 'Verifier', 'UI Overhead']
    times = [
        latency['decompression'],
        latency['circuit_gen'],
        latency['prover'],
        latency['verifier'],
        latency['ui_overhead']
    ]
    
    fig, ax = plt.subplots(figsize=(12, 6))
    
    colors = ['#3498db', '#e74c3c', '#f39c12', '#2ecc71', '#9b59b6']
    
    # Waterfall bars
    cumulative = 0
    for i, (phase, time) in enumerate(zip(phases, times)):
        ax.barh(0, time, left=cumulative, height=0.5, 
                label=f'{phase}: {time:.1f}s', color=colors[i], 
                edgecolor='black', linewidth=1.5)
        # Add time label
        ax.text(cumulative + time/2, 0, f'{time:.1f}s', 
                ha='center', va='center', fontweight='bold', fontsize=10)
        cumulative += time
    
    # UX thresholds
    ax.axvline(3, color='green', linestyle='--', linewidth=2, label='Excellent (< 3s)')
    ax.axvline(10, color='orange', linestyle='--', linewidth=2, label='Acceptable (< 10s)')
    
    # Total time marker
    ax.axvline(cumulative, color='red', linestyle='-', linewidth=3, 
               label=f'Total: {cumulative:.1f}s')
    
    ax.set_xlim(0, max(cumulative * 1.1, 12))
    ax.set_ylim(-0.5, 0.5)
    ax.set_xlabel('Time (seconds)', fontsize=12, fontweight='bold')
    ax.set_title('End-to-End Latency Breakdown (1 Attribute)', 
                 fontsize=14, fontweight='bold')
    ax.set_yticks([])
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(axis='x', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'ux_latency_waterfall.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"✓ Latency waterfall saved")

def create_battery_chart(battery_mah, impact, output_dir):
    """Create battery consumption chart."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Chart 1: mAh consumption
    phones = list(impact.keys())
    consumptions = [battery_mah] * len(phones)
    
    ax1.bar(phones, consumptions, color='#e74c3c', alpha=0.7, edgecolor='black', linewidth=1.5)
    ax1.set_ylabel('Battery Consumption (mAh)', fontsize=12, fontweight='bold')
    ax1.set_title('Battery Consumption per Proof', fontsize=14, fontweight='bold')
    ax1.axhline(50, color='green', linestyle='--', label='Target: < 50 mAh')
    ax1.legend()
    ax1.grid(axis='y', alpha=0.3)
    
    for i, phone in enumerate(phones):
        ax1.text(i, consumptions[i] + 5, f'{consumptions[i]:.1f}', 
                ha='center', fontweight='bold')
    
    # Chart 2: Proofs per charge
    proofs = [impact[phone]['proofs_per_charge'] for phone in phones]
    
    ax2.bar(phones, proofs, color='#2ecc71', alpha=0.7, edgecolor='black', linewidth=1.5)
    ax2.set_ylabel('Proofs per Full Charge', fontsize=12, fontweight='bold')
    ax2.set_title('Proofs per Battery Charge', fontsize=14, fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)
    
    for i, (phone, count) in enumerate(zip(phones, proofs)):
        ax2.text(i, count + 2, f'{count:.0f}', ha='center', fontweight='bold')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'ux_battery_consumption.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"✓ Battery chart saved")

def create_network_chart(network_impact, output_dir):
    """Create network bandwidth impact chart."""
    networks = list(network_impact.keys())
    times = [network_impact[net]['total_time_s'] for net in networks]
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    colors = ['#e74c3c', '#f39c12', '#2ecc71', '#3498db']
    bars = ax.bar(networks, times, color=colors, alpha=0.7, edgecolor='black', linewidth=1.5)
    
    ax.set_ylabel('Total Network Time (seconds)', fontsize=12, fontweight='bold')
    ax.set_title('Network Transfer Time by Connection Type', fontsize=14, fontweight='bold')
    ax.set_yscale('log')
    ax.grid(axis='y', alpha=0.3)
    
    for i, (net, time) in enumerate(zip(networks, times)):
        ax.text(i, time * 1.2, f'{time:.2f}s', ha='center', fontweight='bold')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'ux_network_bandwidth.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"✓ Network chart saved")

def main():
    parser = argparse.ArgumentParser(description='Analyze UX metrics for mDoc ZK wallet')
    parser.add_argument('results_file', help='Path to scalability results JSON')
    parser.add_argument('-o', '--output', default='./ux_analysis', help='Output directory')
    
    args = parser.parse_args()
    
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("Analyzing UX metrics...")
    
    # Latency
    latency = analyze_latency(args.results_file)
    create_latency_waterfall(latency, output_dir)
    
    # Battery
    battery_mah, impact = estimate_battery(latency)
    create_battery_chart(battery_mah, impact, output_dir)
    
    # Network
    network_impact = analyze_network()
    create_network_chart(network_impact, output_dir)
    
    # Summary
    print("\n" + "="*70)
    print("UX METRICS SUMMARY")
    print("="*70)
    print(f"\n📱 Latency:")
    print(f"  Total UX time: {latency['total_ux']:.1f}s")
    print(f"  Breakdown: Decomp {latency['decompression']:.1f}s + "
          f"Circuit {latency['circuit_gen']:.1f}s + "
          f"Prover {latency['prover']:.1f}s + "
          f"Verifier {latency['verifier']:.1f}s")
    
    if latency['total_ux'] < 3:
        print(f"  ✅ EXCELLENT UX (< 3s)")
    elif latency['total_ux'] < 10:
        print(f"  ⚠️  ACCEPTABLE UX (< 10s)")
    else:
        print(f"  ❌ POOR UX (> 10s)")
    
    print(f"\n🔋 Battery:")
    print(f"  Consumption: {battery_mah:.1f} mAh/proof")
    print(f"  iPhone 15: {impact['iPhone 15']['proofs_per_charge']:.0f} proofs/charge "
          f"({impact['iPhone 15']['consumption_pct']:.2f}%)")
    
    print(f"\n📡 Network:")
    print(f"  4G transfer time: {network_impact['4G']['total_time_s']:.2f}s")
    print(f"  5G transfer time: {network_impact['5G']['total_time_s']:.3f}s")
    
    print(f"\n✓ All visualizations saved to: {output_dir}")

if __name__ == '__main__':
    main()
