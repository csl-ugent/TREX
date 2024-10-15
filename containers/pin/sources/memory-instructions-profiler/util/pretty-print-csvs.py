#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output files of the memory instruction profiler Pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/memoryinstructionsprofiler.log')

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

def print_memory_instructions(csv_file):
    reader = csv.DictReader(csv_file)

    def print_readwrite_info(rw):
        print(f'Values ({rw}): {row[rw + "_values"]}')
        print(f'Entropy of values ({rw}): {float(row[rw + "_values_entropy"]):0.6f}')
        print(f'Number of bytes ({rw}): {row["num_bytes_" + rw]}')
        print(f'Number of unique byte addresses ({rw}): {row["num_unique_byte_addresses_" + rw]}')

    for row in reader:
        print(f'Address: {row["full_image_name"]}::{row["section_name"]}::{row["routine_name"]}+{hex(int(row["routine_offset"]))} ({hex(int(row["ip"]))})')
        print_readwrite_info('read')
        print_readwrite_info('written')
        print(f'Number of executions: {row["num_executions"]}')
        print()

# Print memory instructions
print()
print('MEMORY INSTRUCTIONS')
print('===================')
print()

with open(prefix + '.memory-instructions.csv') as f:
    print_memory_instructions(f)
