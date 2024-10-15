#!/usr/bin/env python3

import argparse
import csv
from collections import namedtuple, defaultdict

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output file of the call targets pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/tool.log')

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

def print_call_targets(csv_file):
    CallInstruction = namedtuple("CallInstruction", ["image_name", "image_offset", "filename", "line", "column"])
    Callee = namedtuple("Callee", ["image_name", "image_offset", "function_name", "filename", "line", "column"])

    reader = csv.DictReader(csv_file)
    data = defaultdict(list)

    for row in reader:
        call_inst = CallInstruction(image_name = row['image_name'],
                                    image_offset = row['image_offset'],
                                    filename = row['DEBUG_filename'],
                                    line = row['DEBUG_line'],
                                    column = row['DEBUG_column'])

        callee = Callee(image_name = row['target_image_name'],
                        image_offset = row['target_image_offset'],
                        function_name = row['target_function_name'],
                        filename = row['DEBUG_target_filename'],
                        line = row['DEBUG_target_line'],
                        column = row['DEBUG_target_column'])

        data[call_inst].append(callee)

    for call_inst, callees in data.items():
        print("{0:20}{1}".format("Call:", f"{call_inst.filename}:{call_inst.line}:{call_inst.column} ({call_inst.image_name}+{hex(int(call_inst.image_offset))})"))

        for i, callee in enumerate(callees):
            print("{0:20}{1}".format(f"Callee {i+1}/{len(callees)}:", f"{callee.function_name} ({callee.filename}:{callee.line}:{callee.column}) ({callee.image_name}+{hex(int(callee.image_offset))})"))

        print()

# Print call targets
print()
print('CALL TARGETS')
print('============')
print()

with open(prefix + '.call-targets.csv') as f:
    print_call_targets(f)
