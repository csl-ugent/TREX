from typing import List
from binaryninja import BinaryView, Function, HighlightStandardColor
from mate_attack_framework.util import get_image_name, get_function_containing_address, get_database
import warnings

# Keeps track of the set of highlighted addresses so we can clear the highlights later.
mba_highlighted_addresses = []
mba_highlight_addresses_function = None

def clear_mba_highlights(binary_view: BinaryView):
    '''Clears the highlight of all highlighted instructions.'''
    for addr in mba_highlighted_addresses:
        mba_highlight_addresses_function.set_auto_instr_highlight(addr, HighlightStandardColor.NoHighlightColor)

    mba_highlighted_addresses.clear()

def highlight_mba_instructions(binary_view: BinaryView, addresses: List[int]):
    '''Highlights all instructions in the current function that are part of the mba expression.'''
    global mba_highlighted_addresses
    global mba_highlight_addresses_function

    assert len(addresses) > 0, "The list of addresses to highlight must not be empty"

    function = get_function_containing_address(binary_view, addresses[0])

    # First, clear all highlights.
    clear_mba_highlights(binary_view)

    mba_highlighted_addresses[:] = addresses

    for addr in mba_highlighted_addresses:
        function.set_auto_instr_highlight(addr, HighlightStandardColor.CyanHighlightColor)
    
    mba_highlight_addresses_function = function

addresses_with_comments = set()

def attach_comment_to_mba_instructions(binary_view: BinaryView, addresses: List[int], comment:str):
    global addresses_with_comments
    assert len(addresses) > 0, "The list of addresses to add a comment to must not be empty"

    function = get_function_containing_address(binary_view, addresses[0])
    
    for addr in addresses:
        old_comment = function.get_comment_at(addr)
        if len(old_comment) > 0:
            new_comment = "{old_comment}, {comment}".format(old_comment=old_comment, comment=comment)
        else:
            new_comment = comment
        function.set_comment_at(addr, new_comment)

        addresses_with_comments.add(addr)

def clear_mba_comments(binary_view: BinaryView):
    global addresses_with_comments

    for addr in addresses_with_comments:
        function = get_function_containing_address(binary_view, addr)

        function.set_comment_at(addr, "")