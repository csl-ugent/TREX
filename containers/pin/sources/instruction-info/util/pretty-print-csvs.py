#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output file of the instruction info pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/tool.log')

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

def print_instructions(csv_file):
    reader = csv.DictReader(csv_file)

    # sort by image_offset
    reader = sorted(reader, key=lambda x: int(x['image_offset']))

    for row in reader:
        print(f'Address:        {row["full_image_name"]}::{row["section_name"]}::{row["routine_name"]}+{hex(int(row["routine_offset"]))}')
        print(f'Instruction:    {row["opcode"]} {row["operands"]}')
        print(f'Opcode:         {row["opcode"]}')
        print(f'Operands:       {row["operands"]}')
        print(f'Category:       {row["category"]}')
        print(f'Debug location: {row["DEBUG_filename"]}:{row["DEBUG_line"]}:{row["DEBUG_column"]}')
        print()

# Print instructions
print()
print('INSTRUCTIONS')
print('============')
print()

with open(prefix + '.instruction-info.csv') as f:
    print_instructions(f)
