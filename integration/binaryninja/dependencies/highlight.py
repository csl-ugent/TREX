from binaryninja import *
from mate_attack_framework.util import get_image_name, get_function_containing_address, get_database

# Keeps track of the set of highlighted addresses so we can clear the highlights later.
highlighted_addresses = []
highlighted_addresses_function = None

def clear_highlights(binary_view: BinaryView, function: Function):
    '''Clears the highlight of all highlighted instructions.'''
    global highlighted_addresses_function

    if highlighted_addresses_function is not None:
        for addr in highlighted_addresses:
            highlighted_addresses_function.set_auto_instr_highlight(addr, HighlightStandardColor.NoHighlightColor)

    highlighted_addresses.clear()

def highlight_dependencies(binary_view: BinaryView, instruction_address: int):
    '''Highlights all instructions in the current function that the currently selected instruction depends on.'''
    global highlighted_addresses_function

    db = get_database()

    cur_file = get_image_name(binary_view)
    function = get_function_containing_address(binary_view, instruction_address)

    # First, clear all highlights.
    clear_highlights(binary_view, function)

    addr_lo = function.start
    addr_hi = function.highest_address

    query = f'''
        MATCH p = (ins1:Instruction) -[:DEPENDS_ON]-> (ins2:Instruction)
        WHERE ins1.image_name = "{cur_file}"
        AND ins1.image_offset = {instruction_address}
        AND ALL (
            n IN nodes(p)
            WHERE n.image_name = "{cur_file}"
            AND n.image_offset >= {addr_lo}
            AND n.image_offset <= {addr_hi}
        )
        RETURN DISTINCT (ins2)'''

    for record in db.run_list(query):
        addr = record['ins2']['image_offset']
        highlighted_addresses.append(addr)
        function.set_auto_instr_highlight(addr, HighlightStandardColor.RedHighlightColor)

    highlighted_addresses_function = function
