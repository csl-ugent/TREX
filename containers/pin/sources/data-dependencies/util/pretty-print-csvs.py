#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the CSV output files of the data dependencies Pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix')

parser.add_argument('--prefix', help='Prefix path of the CSV files', required=True)

args = parser.parse_args()

# Read arguments
prefix = args.prefix

def print_syscall_ins(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Instruction: {row["image_name"]}+{hex(int(row["image_offset"]))} (syscall index {row["index"]})')

def print_register_deps(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Instructions: {row["Write_img"]}+{hex(int(row["Write_off"]))} <- {row["Read_img"]}+{hex(int(row["Read_off"]))}')
        print(f'Register:     {row["Register"]}')
        print()

def print_memory_deps(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Instructions:   {row["Write_img"]}+{hex(int(row["Write_off"]))} <- {row["Read_img"]}+{hex(int(row["Read_off"]))}')
        print(f'Memory address: {hex(int(row["Memory"]))}')
        print()

# Print syscall instructions
print()
print('SYSTEM CALL INSTRUCTIONS')
print('========================')
print()

with open(prefix + '.syscall_instructions.csv') as f:
    print_syscall_ins(f)

# Print register deps
print()
print('REGISTER DEPENDENCIES')
print('=====================')
print()

with open(prefix + '.register_dependencies.csv') as f:
    print_register_deps(f)

# Print memory deps
print()
print('MEMORY DEPENDENCIES')
print('===================')
print()

with open(prefix + '.memory_dependencies.csv') as f:
    print_memory_deps(f)
