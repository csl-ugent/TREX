#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output file of the syscall trace pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/systemcalls.log')

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


def print_strace(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        name = row["name"]
        num_arguments = int(row["num_arguments"])
        args = [row[f"argument_{i}"] for i in range(0, num_arguments)]
        retval = row["return_value"]

        print(f'{name}({", ".join(args)}) = {retval}')


# Print instructions
print()
print('SYSTEM CALLS')
print('============')
print()

with open(prefix + '.system-calls.csv') as f:
    print_strace(f)
