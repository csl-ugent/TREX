#!/usr/bin/env python3

"""
Patches files (e.g. CSV files output by the Pintools) to include debug information.

The script looks for any occurrence of the following pattern:
    $DEBUG(image_name,image_offset,type)
and replaces it by the correct debug info by parsing the DWARF.

"type" can be either "filename", "line", or "column".
"""

import argparse
import os
import re
import sys
import intervaltree
from collections import defaultdict

from elftools.elf.elffile import ELFFile

parser = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('infile', nargs='?', type=argparse.FileType('r'),
                    default=sys.stdin, help='Input file (default: stdin)')
parser.add_argument('-o', metavar='outfile', type=argparse.FileType('w'),
                    default=sys.stdout, help='Output file (default: stdout)',
                    dest='outfile')
args = parser.parse_args()


def get_dwarf_info_for_image(image_name):
    with open(image_name, 'rb') as f:
        elffile = ELFFile(f)
        if not elffile.has_dwarf_info():
            return None
        else:
            return elffile.get_dwarf_info()

# Cache results for different offsets for performance reasons.
debug_info_cache = defaultdict(lambda: intervaltree.IntervalTree())


def populate_debug_info_cache(image_name, dwarfinfo):
    if dwarfinfo is None:
        debug_info_cache[image_name] = None
        return

    # NOTE: based on the dwarf_decode_address.py example of pyelftools.
    # Go over all the line programs in the DWARF information, looking for
    # one that describes the given address.
    for CU in dwarfinfo.iter_CUs():
        # Get the DWARF version
        dwarf_version = CU['version']

        # First, look at line programs to find the file/line for the address
        lineprog = dwarfinfo.line_program_for_CU(CU)
        prevstate = None
        for entry in lineprog.get_entries():
            # We're interested in those entries where a new state is assigned
            if entry.state is None:
                continue
            # Looking for a range of addresses in two consecutive states that
            # contain the required address.
            if prevstate:
                # NOTE: See the example dwarf_lineprogram_filenames.py
                file_entry = lineprog['file_entry'][prevstate.file - 1]
                dir_index = file_entry['dir_index']

                # The meaning of dir_index is different in DWARF4 and DWARF5,
                # compare Section 6.2 (p. 114) of the DWARF4 standard with
                # Section 6.2 (p. 157) of the DWARF5 standard.
                if dwarf_version < 5:
                    if dir_index == 0:
                        # No absolute directory recorded, so just take the
                        # basename.
                        filename = file_entry.name.decode('latin-1')
                    else:
                        dirname = lineprog['include_directory'][dir_index - 1]
                        base_filename = file_entry.name
                        filename = os.path.join(
                            dirname, base_filename).decode('latin-1')
                else:
                    dirname = lineprog['include_directory'][dir_index]
                    base_filename = file_entry.name
                    filename = os.path.join(
                        dirname, base_filename).decode('latin-1')
                line = prevstate.line
                col = prevstate.column

                addr_lo = prevstate.address
                addr_hi = entry.state.address

                if addr_lo < addr_hi:
                    debug_info_cache[image_name][addr_lo:addr_hi] = (filename, line, col)
            if entry.state.end_sequence:
                # For the state with `end_sequence`, `address` means the address
                # of the first byte after the target machine instruction
                # sequence and other information is meaningless. We clear
                # prevstate so that it's not used in the next iteration. Address
                # info is used in the above comparison to see if we need to use
                # the line information for the prevstate.
                prevstate = None
            else:
                prevstate = entry.state


def decode_file_line_col(image_name, image_offset):
    if image_name not in debug_info_cache:
        return '???', 0, 0

    entry = debug_info_cache[image_name][image_offset]

    if len(entry) == 0:
        return '???', 0, 0

    # grab the first item of the set
    first_entry = next(iter(entry))

    return first_entry.data


def get_debug_info(image_name, image_offset, type):
    # Skip unknown images
    if image_name == '???':
        file, line, col = '???', 0, 0
    else:
        if image_name not in debug_info_cache:
            dwarf_info = get_dwarf_info_for_image(image_name)
            populate_debug_info_cache(image_name, dwarf_info)

        file, line, col = decode_file_line_col(image_name, int(image_offset))

    if type == 'filename':
        return file
    elif type == 'line':
        return str(line)
    elif type == 'column':
        return str(col)
    else:
        raise ValueError(f'Unknown type: {type}')


prog = re.compile(r'\$DEBUG\(([^,)]*),([^,)]*),([^,)]*)\)')

for line in args.infile:
    args.outfile.write(prog.sub(lambda matchobj: get_debug_info(
        *matchobj.group(1, 2, 3)), line))
