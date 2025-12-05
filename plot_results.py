#!/usr/bin/env python3
"""
Plot Hamming Distance/Weight results from CSV files.

Usage:
    python plot_results.py input.csv output.png [--title "Title"]
"""

import argparse
import csv
import matplotlib.pyplot as plt
import sys
from pathlib import Path


def read_csv(filename):
    """Read timestamp,value pairs from CSV file."""
    timestamps = []
    values = []
    
    with open(filename, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if len(row) == 2:
                timestamps.append(int(row[0]))
                values.append(int(row[1]))
    
    return timestamps, values


def plot_results(csv_file, output_file, title=None):
    """Plot results and save to file."""
    timestamps, values = read_csv(csv_file)
    
    if len(timestamps) == 0:
        print(f"Error: No data found in {csv_file}", file=sys.stderr)
        sys.exit(1)
    
    # Create figure
    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, values, linewidth=1.5, color='#2563eb')
    
    # Set labels
    plt.xlabel('Timestamp', fontsize=12)
    plt.ylabel('Value', fontsize=12)
    
    # Set title
    if title:
        plt.title(title, fontsize=14, fontweight='bold')
    else:
        # Auto-generate title from filename
        csv_name = Path(csv_file).stem
        plt.title(f'Results: {csv_name}', fontsize=14, fontweight='bold')
    
    # Grid for better readability
    plt.grid(True, alpha=0.3, linestyle='--')
    
    # Tight layout
    plt.tight_layout()
    
    # Save figure
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Plot saved to: {output_file}")
    print(f"  Data points: {len(timestamps)}")
    print(f"  Timestamp range: {timestamps[0]} - {timestamps[-1]}")
    print(f"  Value range: {min(values)} - {max(values)}")


def main():
    parser = argparse.ArgumentParser(
        description='Plot Hamming Distance/Weight results from CSV files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python plot_results.py results.csv plot.png
  python plot_results.py results.csv plot.pdf --title "Hamming Distance: AES"
  python plot_results.py hd_results.csv figure.svg --title "Custom Title"
        """
    )
    
    parser.add_argument('input_csv', help='Input CSV file with timestamp,value pairs')
    parser.add_argument('output_figure', help='Output figure file (png, pdf, svg, etc.)')
    parser.add_argument('--title', '-t', help='Plot title (default: auto-generated from filename)')
    
    args = parser.parse_args()
    
    # Check input file exists
    if not Path(args.input_csv).exists():
        print(f"Error: Input file not found: {args.input_csv}", file=sys.stderr)
        sys.exit(1)
    
    plot_results(args.input_csv, args.output_figure, args.title)


if __name__ == '__main__':
    main()
