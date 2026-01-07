#!/usr/bin/env python3
"""
Scalability Analyzer for mDoc ZK Benchmarks

Analyzes benchmark results to determine complexity (linear, quadratic, exponential)
and generates publication-ready graphs and tables.
"""

import json
import argparse
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
from scipy import stats
from scipy.optimize import curve_fit
import sys

# Styling
plt.style.use('seaborn-v0_8-darkgrid')
COLORS = ['#2E86AB', '#A23B72', '#F18F01', '#C73E1D']


def load_benchmark_results(json_file):
    """Load and parse Google Benchmark JSON results."""
    with open(json_file, 'r') as f:
        data = json.load(f)
    return data


def extract_scalability_data(data):
    """Extract scalability metrics from benchmark results."""
    benchmarks = data.get('benchmarks', [])
    
    results = []
    for bm in benchmarks:
        if 'Scalability' in bm['name']:
            num_attrs = int(bm.get('num_attributes', 0))
            
            result = {
                'num_attributes': num_attrs,
                'circuit_size': bm.get('circuit_size_bytes', 0),
                'proof_size': bm.get('proof_size_bytes', 0),
                'circuit_gen_time': bm.get('circuit_gen_ns', 0) / 1e9,  # Convert to seconds
                'prover_time': bm.get('prover_ns', 0) / 1e9,
                'verifier_time': bm.get('verifier_ns', 0) / 1e9,
                'total_time': bm.get('total_ns', 0) / 1e9,
                'proof_bytes_per_attr': bm.get('proof_bytes_per_attr', 0),
                'prover_verifier_ratio': bm.get('prover_verifier_ratio', 0),
            }
            results.append(result)
    
    # Sort by number of attributes
    results.sort(key=lambda x: x['num_attributes'])
    return results


def linear_model(x, a, b):
    """Linear model: y = ax + b"""
    return a * x + b


def quadratic_model(x, a, b, c):
    """Quadratic model: y = ax² + bx + c"""
    return a * x**2 + b * x + c


def exponential_model(x, a, b):
    """Exponential model: y = a * e^(bx)"""
    return a * np.exp(b * x)


def fit_models(x, y, metric_name):
    """Fit linear, quadratic, and exponential models and return best fit."""
    models = {}
    
    try:
        # Linear fit
        popt_lin, _ = curve_fit(linear_model, x, y)
        y_pred_lin = linear_model(x, *popt_lin)
        r2_lin = 1 - (np.sum((y - y_pred_lin)**2) / np.sum((y - np.mean(y))**2))
        models['linear'] = {
            'params': popt_lin,
            'r2': r2_lin,
            'equation': f'y = {popt_lin[0]:.2e}x + {popt_lin[1]:.2e}',
            'predict': lambda x_val: linear_model(x_val, *popt_lin)
        }
    except Exception as e:
        print(f"Linear fit failed for {metric_name}: {e}")
    
    try:
        # Quadratic fit
        popt_quad, _ = curve_fit(quadratic_model, x, y)
        y_pred_quad = quadratic_model(x, *popt_quad)
        r2_quad = 1 - (np.sum((y - y_pred_quad)**2) / np.sum((y - np.mean(y))**2))
        models['quadratic'] = {
            'params': popt_quad,
            'r2': r2_quad,
            'equation': f'y = {popt_quad[0]:.2e}x² + {popt_quad[1]:.2e}x + {popt_quad[2]:.2e}',
            'predict': lambda x_val: quadratic_model(x_val, *popt_quad)
        }
    except Exception as e:
        print(f"Quadratic fit failed for {metric_name}: {e}")
    
    try:
        # Exponential fit
        popt_exp, _ = curve_fit(exponential_model, x, y, p0=[1, 0.1], maxfev=10000)
        y_pred_exp = exponential_model(x, *popt_exp)
        r2_exp = 1 - (np.sum((y - y_pred_exp)**2) / np.sum((y - np.mean(y))**2))
        models['exponential'] = {
            'params': popt_exp,
            'r2': r2_exp,
            'equation': f'y = {popt_exp[0]:.2e} * e^({popt_exp[1]:.2e}x)',
            'predict': lambda x_val: exponential_model(x_val, *popt_exp)
        }
    except Exception as e:
        print(f"Exponential fit failed for {metric_name}: {e}")
    
    # Find best model
    if models:
        best_model_name = max(models.keys(), key=lambda k: models[k]['r2'])
        return models, best_model_name
    return {}, None


def plot_time_vs_attributes(results, output_dir):
    """Plot execution time vs number of attributes with regression curves."""
    x = np.array([r['num_attributes'] for r in results])
    
    metrics = {
        'Circuit Generation': [r['circuit_gen_time'] for r in results],
        'Proof Generation (Prover)': [r['prover_time'] for r in results],
        'Verification': [r['verifier_time'] for r in results],
        'Total Time': [r['total_time'] for r in results],
    }
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    analysis_results = {}
    
    for idx, (metric_name, y_data) in enumerate(metrics.items()):
        ax = axes[idx]
        y = np.array(y_data)
        
        # Plot actual data
        ax.scatter(x, y, s=100, color=COLORS[idx], label='Measured', zorder=5)
        
        # Fit models
        models, best_model = fit_models(x, y, metric_name)
        analysis_results[metric_name] = {'models': models, 'best': best_model}
        
        # Plot regression curves
        x_smooth = np.linspace(x.min(), x.max(), 100)
        
        if 'linear' in models:
            y_lin = models['linear']['predict'](x_smooth)
            ax.plot(x_smooth, y_lin, '--', label=f"Linear (R²={models['linear']['r2']:.4f})", alpha=0.7)
        
        if 'quadratic' in models:
            y_quad = models['quadratic']['predict'](x_smooth)
            ax.plot(x_smooth, y_quad, '-.', label=f"Quadratic (R²={models['quadratic']['r2']:.4f})", alpha=0.7)
        
        if 'exponential' in models and models['exponential']['r2'] > 0:
            y_exp = models['exponential']['predict'](x_smooth)
            ax.plot(x_smooth, y_exp, ':', label=f"Exponential (R²={models['exponential']['r2']:.4f})", alpha=0.7)
        
        ax.set_xlabel('Number of Attributes', fontsize=12, fontweight='bold')
        ax.set_ylabel('Time (seconds)', fontsize=12, fontweight='bold')
        ax.set_title(metric_name, fontsize=14, fontweight='bold')
        ax.legend(loc='best')
        ax.grid(True, alpha=0.3)
        
        # Add best fit equation as text
        if best_model and best_model in models:
            textstr = f'Best Fit ({best_model}):\n{models[best_model]["equation"]}'
            ax.text(0.05, 0.95, textstr, transform=ax.transAxes, fontsize=9,
                   verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    output_path = output_dir / 'time_vs_attributes.png'
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"✓ Time vs Attributes plot saved to: {output_path}")
    plt.close()
    
    return analysis_results


def plot_size_vs_attributes(results, output_dir):
    """Plot proof and circuit size vs number of attributes."""
    x = np.array([r['num_attributes'] for r in results])
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # Proof Size
    y_proof = np.array([r['proof_size'] / 1024 / 1024 for r in results])  # Convert to MB
    ax1.scatter(x, y_proof, s=100, color=COLORS[0], label='Measured', zorder=5)
    
    models_proof, best_proof = fit_models(x, y_proof, 'Proof Size')
    if best_proof and best_proof in models_proof:
        x_smooth = np.linspace(x.min(), x.max(), 100)
        y_pred = models_proof[best_proof]['predict'](x_smooth)
        ax1.plot(x_smooth, y_pred, '--', label=f"{best_proof.capitalize()} fit (R²={models_proof[best_proof]['r2']:.4f})")
    
    ax1.set_xlabel('Number of Attributes', fontsize=12, fontweight='bold')
    ax1.set_ylabel('Proof Size (MB)', fontsize=12, fontweight='bold')
    ax1.set_title('Proof Size vs Number of Attributes', fontsize=14, fontweight='bold')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Circuit Size
    y_circuit = np.array([r['circuit_size'] / 1024 / 1024 for r in results])  # Convert to MB
    ax2.scatter(x, y_circuit, s=100, color=COLORS[1], label='Measured', zorder=5)
    
    models_circuit, best_circuit = fit_models(x, y_circuit, 'Circuit Size')
    if best_circuit and best_circuit in models_circuit:
        x_smooth = np.linspace(x.min(), x.max(), 100)
        y_pred = models_circuit[best_circuit]['predict'](x_smooth)
        ax2.plot(x_smooth, y_pred, '--', label=f"{best_circuit.capitalize()} fit (R²={models_circuit[best_circuit]['r2']:.4f})")
    
    ax2.set_xlabel('Number of Attributes', fontsize=12, fontweight='bold')
    ax2.set_ylabel('Circuit Size (MB)', fontsize=12, fontweight='bold')
    ax2.set_title('Circuit Size vs Number of Attributes', fontsize=14, fontweight='bold')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    output_path = output_dir / 'size_vs_attributes.png'
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"✓ Size vs Attributes plot saved to: {output_path}")
    plt.close()


def plot_ratios(results, output_dir):
    """Plot performance ratios."""
    x = np.array([r['num_attributes'] for r in results])
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # Prover/Verifier Ratio
    y_ratio = np.array([r['prover_verifier_ratio'] for r in results])
    ax1.plot(x, y_ratio, 'o-', linewidth=2, markersize=10, color=COLORS[2])
    ax1.set_xlabel('Number of Attributes', fontsize=12, fontweight='bold')
    ax1.set_ylabel('Prover Time / Verifier Time', fontsize=12, fontweight='bold')
    ax1.set_title('Prover/Verifier Time Ratio', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    
    # Bytes per Attribute
    y_bytes = np.array([r['proof_bytes_per_attr'] / 1024 for r in results])  # Convert to KB
    ax2.plot(x, y_bytes, 's-', linewidth=2, markersize=10, color=COLORS[3])
    ax2.set_xlabel('Number of Attributes', fontsize=12, fontweight='bold')
    ax2.set_ylabel('Proof Size per Attribute (KB)', fontsize=12, fontweight='bold')
    ax2.set_title('Proof Size Efficiency', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    output_path = output_dir / 'performance_ratios.png'
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"✓ Performance ratios plot saved to: {output_path}")
    plt.close()


def generate_latex_table(results, analysis_results, output_dir):
    """Generate LaTeX table for publication."""
    latex = []
    latex.append("\\begin{table}[h]")
    latex.append("\\centering")
    latex.append("\\caption{mDoc ZK Proof System Scalability Analysis}")
    latex.append("\\label{tab:scalability}")
    latex.append("\\begin{tabular}{|c|c|c|c|c|c|c|}")
    latex.append("\\hline")
    latex.append("\\textbf{\\#Attrs} & \\textbf{Circuit (MB)} & \\textbf{Proof (MB)} & \\textbf{Circuit Gen (s)} & \\textbf{Prover (s)} & \\textbf{Verifier (s)} & \\textbf{P/V Ratio} \\\\")
    latex.append("\\hline")
    
    for r in results:
        latex.append(f"{r['num_attributes']} & "
                    f"{r['circuit_size']/1024/1024:.2f} & "
                    f"{r['proof_size']/1024/1024:.2f} & "
                    f"{r['circuit_gen_time']:.2f} & "
                    f"{r['prover_time']:.2f} & "
                    f"{r['verifier_time']:.2f} & "
                    f"{r['prover_verifier_ratio']:.2f} \\\\")
    
    latex.append("\\hline")
    latex.append("\\end{tabular}")
    latex.append("\\end{table}")
    
    latex_content = "\n".join(latex)
    output_path = output_dir / 'scalability_table.tex'
    with open(output_path, 'w') as f:
        f.write(latex_content)
    print(f"✓ LaTeX table saved to: {output_path}")
    
    # Also print complexity analysis
    print("\n" + "="*70)
    print("COMPLEXITY ANALYSIS")
    print("="*70)
    for metric, data in analysis_results.items():
        print(f"\n{metric}:")
        if data['models']:
            for model_name, model_data in data['models'].items():
                print(f"  {model_name.capitalize()}: R² = {model_data['r2']:.4f}")
                print(f"    Equation: {model_data['equation']}")
            if data['best']:
                print(f"  ✓ Best fit: {data['best'].upper()} (R² = {data['models'][data['best']]['r2']:.4f})")


def main():
    parser = argparse.ArgumentParser(description='Analyze mDoc ZK scalability benchmarks')
    parser.add_argument('json_file', help='Path to benchmark JSON results')
    parser.add_argument('-o', '--output', default='./analysis', help='Output directory for plots')
    
    args = parser.parse_args()
    
    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Loading benchmark results from: {args.json_file}")
    data = load_benchmark_results(args.json_file)
    
    results = extract_scalability_data(data)
    if not results:
        print("Error: No scalability benchmark results found in JSON file")
        sys.exit(1)
    
    print(f"Found {len(results)} benchmark result(s)")
    
    # Generate plots
    print("\nGenerating plots...")
    analysis_results = plot_time_vs_attributes(results, output_dir)
    plot_size_vs_attributes(results, output_dir)
    plot_ratios(results, output_dir)
    
    # Generate LaTeX table
    print("\nGenerating LaTeX table...")
    generate_latex_table(results, analysis_results, output_dir)
    
    print(f"\n✓ All analysis artifacts saved to: {output_dir}")


if __name__ == '__main__':
    main()
