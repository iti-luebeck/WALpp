#!/usr/bin/env python3
"""
Plot time-series data from WAL operator output.
Parses output format: ((timestamp1 value1) (timestamp2 value2) ...)
Works with any WAL operator output (hamming-distance, hamming-weight, etc.)
"""

import re
import matplotlib.pyplot as plt
import argparse
import os


def parse_output(filename):
    """Parse the output file containing (timestamp value) tuples."""
    with open(filename, 'r') as f:
        content = f.read().strip()
    
    # Extract all (timestamp value) pairs using regex
    pattern = r'\((\d+)\s+(\d+)\)'
    matches = re.findall(pattern, content)
    
    timestamps = []
    values = []
    
    for timestamp, value in matches:
        timestamps.append(int(timestamp))
        values.append(int(value))
    
    return timestamps, values


def plot_data(timestamps, values, output_file, title=None):
    """Create a plot of values over time."""
    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, values, linewidth=0.8, marker='o', markersize=2, alpha=0.7)
    
    plt.xlabel('Timestamp', fontsize=12)
    plt.ylabel('Value', fontsize=12)
    
    # Use provided title or derive from output filename
    if title:
        plot_title = title
    else:
        plot_title = os.path.splitext(os.path.basename(output_file))[0].replace('_', ' ').title()
    
    plt.title(plot_title, fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    
    # Format x-axis to show readable timestamps
    plt.ticklabel_format(style='plain', axis='x')
    plt.xticks(rotation=45)
    
    plt.tight_layout()
    
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Plot saved to {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Plot time-series data from WAL operator output (hamming-distance, hamming-weight, etc.)'
    )
    parser.add_argument(
        'input_file',
        help='Input file containing (timestamp value) tuples'
    )
    parser.add_argument(
        'output_file',
        help='Output file for plot (e.g., plot.png, graph.pdf)'
    )
    parser.add_argument(
        '-t', '--title',
        help='Custom title for the plot',
        default=None
    )
    
    args = parser.parse_args()
    
    print(f"Parsing {args.input_file}...")
    timestamps, values = parse_output(args.input_file)
    
    print(f"Found {len(timestamps)} data points")
    print(f"Time range: {timestamps[0]} to {timestamps[-1]}")
    print(f"Value range: {min(values)} to {max(values)}")
    
    plot_data(timestamps, values, args.output_file, args.title)


if __name__ == '__main__':
    main()
