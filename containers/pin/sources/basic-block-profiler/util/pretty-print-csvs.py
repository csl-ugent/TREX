#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output file of the basic block profiler pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/tool.log')

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

def print_basic_blocks(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Address range:        {row["full_image_name"]}::{row["section_name"]}::{row["routine_name"]}+{hex(int(row["routine_offset_begin"]))} --> {row["full_image_name"]}::{row["section_name"]}::{row["routine_name"]}+{hex(int(row["routine_offset_end"]))}')
        print(f'Number of executions: {row["num_executions"]}')
        print()

# Print basic blocks
print()
print('BASIC BLOCKS')
print('============')
print()

with open(prefix + '.basic-blocks.csv') as f:
    print_basic_blocks(f)
