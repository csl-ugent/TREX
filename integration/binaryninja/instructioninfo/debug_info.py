from binaryninja import *
from mate_attack_framework.util import get_image_name, get_database

def add_debug_info_comments(binary_view: BinaryView, function: Function):
    '''Add filename, line number, and column number from debug information as
    comments to the instructions in a given function.'''
    db = get_database()
    cur_file = get_image_name(binary_view)
    addr_lo = function.start
    addr_hi = function.highest_address

    instructions = db.run_list(f'''
        MATCH (ins:Instruction)
        WHERE ins.image_name CONTAINS "{cur_file}"
        AND ins.image_offset >= {addr_lo}
        AND ins.image_offset <= {addr_hi}
        RETURN (ins)''')

    for entry in instructions:
        offset = int(entry['ins']['image_offset'])
        file = entry['ins']['DEBUG_filename']
        line = int(entry['ins']['DEBUG_line'])
        col = int(entry['ins']['DEBUG_column'])

        comment = binary_view.get_comment_at(offset)
        comment += f'{file}:{line}:{col}\n'
        binary_view.set_comment_at(offset, comment)
