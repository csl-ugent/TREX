#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output file of the instruction values pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/tool.log')

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

    for row in reader:
        print(f'Instruction:              {row["image_name"]}+{hex(int(row["image_offset"]))}')
        print(f'Number of operands:       {row["num_operands"]}')
        print()

        for i in range(0, int(row['num_operands'])):
            # Replace [] by (), because the former are special characters in FileCheck.
            print(f'Operand {i} repr:           {row["operand_" + str(i) + "_repr"].replace("[", "(").replace("]", ")")}')
            print(f'Operand {i} width:          {row["operand_" + str(i) + "_width"]}')
            print(f'Operand {i} is read:        {int(row["operand_" + str(i) + "_is_read"]) != 0}')
            print(f'Operand {i} is written:     {int(row["operand_" + str(i) + "_is_written"]) != 0}')
            print(f'Operand {i} read values:    [{row["operand_" + str(i) + "_read_values"]}]')
            print(f'Operand {i} written values: [{row["operand_" + str(i) + "_written_values"]}]')
            print()

        print()
        print()

# Print instructions
print()
print('INSTRUCTIONS')
print('============')
print()

with open(prefix + '.instruction-values.csv') as f:
    print_instructions(f)
