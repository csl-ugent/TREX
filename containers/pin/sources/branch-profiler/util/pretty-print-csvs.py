#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output file of the Branch Profiler Pintool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/branchprofiler.log')

parser.add_argument('--prefix', help='Prefix path of the CSV file', required=True)
parser.add_argument('--log_file', help='Path to the log file', required=False, default='')

args = parser.parse_args()

# Read arguments
log_file = args.log_file
prefix = args.prefix

# Print log file
if log_file != '':
    with open(log_file) as f:
        for line in f:
            print(line, end='')

def print_branches(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Instruction:     {row["image_name"]}+{hex(int(row["image_offset"]))}')
        print(f'Times taken:     {row["num_taken"]}')
        print(f'Times not taken: {row["num_not_taken"]}')
        print()

# Print branches blocks
print()
print('BRANCHES')
print('========')
print()

with open(prefix + '.branches.csv') as f:
    print_branches(f)
