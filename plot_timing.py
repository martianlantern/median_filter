#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read the CSV data
df = pd.read_csv('timing_results.csv')

# Create the plot
plt.figure(figsize=(12, 8))

# Get unique versions and assign colors
versions = df['Version'].unique()
colors = plt.cm.Set1(np.linspace(0, 1, len(versions)))

for i, version in enumerate(versions):
    version_data = df[df['Version'] == version]
    
    plt.errorbar(version_data['KernelSize'], version_data['MeanTime'], 
                yerr=version_data['StdTime'],
                label=version, marker='o', capsize=5, 
                color=colors[i], linewidth=2, markersize=6)

plt.xlabel('Kernel Size (pixels)', fontsize=12, fontweight='bold')
plt.ylabel('Average Runtime (ms)', fontsize=12, fontweight='bold')
plt.title('Median Filter Performance Comparison\n500x500 Image, 5 Runs Average', 
          fontsize=14, fontweight='bold')
plt.legend(fontsize=11, loc='upper left')
plt.grid(True, alpha=0.3)
plt.yscale('log')  # Log scale for better visualization of different performance levels

# Customize the plot
plt.tight_layout()

# Save the plot
plt.savefig('median_filter_timing.png', dpi=300, bbox_inches='tight')
plt.savefig('median_filter_timing.pdf', bbox_inches='tight')

print("Plot saved as 'median_filter_timing.png' and 'median_filter_timing.pdf'")

# Show the plot
plt.show()
