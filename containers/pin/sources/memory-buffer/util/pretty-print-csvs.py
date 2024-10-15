#!/usr/bin/env python3

import argparse
import csv

# Parse arguments
parser = argparse.ArgumentParser(description = 'Helper script to print the contents of the human readable output log file and the CSV output files of the memory buffer Pin tool', epilog='example: pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/tool.log')

parser.add_argument('--prefix', help='Prefix path of the CSV files', required=True)
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

def print_static_instructions(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Address:                 {row["full_image_name"]}::{row["section_name"]}::{row["routine_name"]}+{hex(int(row["routine_offset"]))} ({hex(int(row["ip"]))})')
        print(f'Disassembly:             {row["opcode"]:15} {row["operands"]}')
        print(f'Read values:             {row["read_values"]}')
        print(f'Written values:          {row["written_values"]}')
        print(f'Read values entropy:     {row["read_values_entropy"]}')
        print(f'Written values entropy:  {row["written_values_entropy"]}')
        print(f'Number of bytes read:    {row["num_bytes_read"]}')
        print(f'Number of bytes written: {row["num_bytes_written"]}')
        print()

def pretty_ip_list(ip_list):
    if ip_list == '':
        return '[]'

    hex_ips = [hex(int(ip)) for ip in ip_list.split(',')]
    return '[' + ', '.join(hex_ips) + ']'

def print_memory(csv_file):
    reader = csv.DictReader(csv_file)

    for row in reader:
        print(f'Buffer:                                 Buffer {hex(int(row["start_address"]))} --> {hex(int(row["end_address"]))}')
        print(f'Number of reads:                        {row["num_reads"]}')
        print(f'Number of writes:                       {row["num_writes"]}')

        for metric in ['bit_shannon', 'byte_shannon', 'byte_shannon_adapted', 'byte_num_different', 'byte_num_unique', 'bit_average', 'byte_average']:
            print(f'Average spatial entropy ({metric}): {row["average_spatial_entropy_" + metric]}')

        for metric in ['bit_shannon']:
            print(f'Average temporal entropy ({metric}): {row["average_temporal_entropy_" + metric]}')

        print(f'Read instructions:                      {pretty_ip_list(row["read_ips"])}')
        print(f'Write instructions:                     {pretty_ip_list(row["write_ips"])}')
        print(f'Allocation address:                     {row["allocation_address"]}')
        print(f'Annotation:                             {row["DEBUG_annotation"]}')
        print()

# Print static instructions
print()
print('STATIC INSTRUCTIONS')
print('===================')
print()

with open(prefix + '.instructions.csv') as f:
    print_static_instructions(f)

# Print memory buffers
print()
print('MEMORY BUFFERS')
print('==============')
print()

with open(prefix + '.buffers.csv') as f:
    print_memory(f)

# Print memory regions
print()
print('MEMORY REGIONS')
print('==============')
print()

with open(prefix + '.regions.csv') as f:
    print_memory(f)
