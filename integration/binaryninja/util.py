import os

from binaryninja import *

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from binaryninjaui import CrossReferenceItemDelegate

from mate_attack_framework.neo4j_py2neo_bridge import Graph, GraphService

from enum import Enum

from PySide6.QtGui import QIcon, QPixmap
from PySide6.QtWidgets import QHeaderView, QTableWidgetItem, QTableWidget, QHeaderView

def get_image_name(binary_view: BinaryView):
    '''
    Get the basename of the image currently opened in a binary view.
    This corresponds to the basename of the 'image_name' in the graph database.
    '''
    return os.path.basename(binary_view.file.filename)

def get_function_containing_address(binary_view: BinaryView, address: int):
    '''
    Get the function containing an instruction with a given address.
    Returns 'None' if no such function is found.
    '''
    functions = binary_view.get_functions_containing(address)
    return functions[0] if len(functions) > 0 else None

def switch_database():
    '''Switches the currently active database.'''

    # Get the list of graphs in the database.
    gs = GraphService(f'bolt://localhost:7687', auth=('neo4j', 'test'))
    graphs = [graph for graph in gs if graph not in ['neo4j', 'system']]

    # Prompt the user for the graph to use.
    name = graphs[get_choice_input('Select workspace:', 'Select workspace', graphs)]

    # Set the graph.
    get_database.db = gs[name]

def get_database():
    '''Returns a handle to the graph database.'''
    if not hasattr(get_database, 'db'):
        switch_database()

    return get_database.db

def get_instruction_string_rep(function: Function, address: int):
    '''Gets the string representation of an instruction.'''
    for ins in function.instructions:
        ins_addr = ins[1]

        if ins_addr == address:
            return ''.join([str(i) for i in ins[0]])

class XrefImageDirection(Enum):
    LEFT_TO_RIGHT = 1
    RIGHT_TO_LEFT = 2

def get_crossreference_icon(direction: XrefImageDirection):
    '''Returns the icon used by Binary Ninja for cross references.'''
    delegate = CrossReferenceItemDelegate(None, False)

    if direction == XrefImageDirection.LEFT_TO_RIGHT:
        return QIcon(QPixmap.fromImage(delegate.DrawArrow(True)))
    elif direction == XrefImageDirection.RIGHT_TO_LEFT:
        return QIcon(QPixmap.fromImage(delegate.DrawArrow(False)))
    else:
        raise ValueError('Invalid XrefImageDirection')

def little_endian_byte_str_to_num(byte_str):
    '''
    Converts a little-endian hexadecimal byte string (e.g. '01 00 00 00') to
    a number (e.g. 1).
    '''

    byte_arr = byte_str.strip().split(' ')

    # swap byte order for little-endian
    byte_arr.reverse()

    # convert to number
    return int(''.join(byte_arr), 16)

def set_headers(widget: QTableWidget, headers: list):
    '''Set the headers of a QTableWidget'''
    widget.setColumnCount(len(headers))
    widget.setHorizontalHeaderLabels(headers)
    widget.verticalHeader().hide()
    widget.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)

def set_items(widget: QTableWidget, items: list):
    '''Set the items of a QTableWidget'''
    widget.clearContents()
    widget.setRowCount(0)

    # We need to disable sorting temporarily when inserting items
    old_sorting = widget.isSortingEnabled()
    widget.setSortingEnabled(False)

    for item in items:
        row = widget.rowCount()
        widget.insertRow(row)

        for col, value in enumerate(item):
            if isinstance(value, QTableWidgetItem):
                widget.setItem(row, col, value)
            else:
                widget.setItem(row, col, QTableWidgetItem(str(value)))

    # Restore sorting
    widget.setSortingEnabled(old_sorting)
