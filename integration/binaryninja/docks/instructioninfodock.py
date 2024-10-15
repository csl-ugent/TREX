import os

from mate_attack_framework.ui.ui_instructioninfodock import Ui_InstructionInfoDock
from mate_attack_framework.util import get_function_containing_address, get_image_name, get_database, little_endian_byte_str_to_num, set_headers, set_items
from mate_attack_framework.dependencies.highlight import clear_highlights, highlight_dependencies
from mate_attack_framework.docks.dockbase import DockBase

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from binaryninjaui import DockContextHandler, UIActionHandler

from PySide6.QtWidgets import QWidget, QHeaderView, QTableWidgetItem

def _byte_str_numbits(byte_str):
    '''Get the number of bits in a byte string.'''
    count = 0

    for char in byte_str:
        if char in '0123456789ABCDEFabcdef':
            count += 4

    return count

class InstructionInfoDock(DockBase):
    def __init__(self, parent, name):
        DockBase.__init__(self, parent, name, ui = Ui_InstructionInfoDock())

        for widget in [self.ui.read_values_tablewidget, self.ui.written_values_tablewidget]:
            # TODO: Add column with spatial entropy?
            set_headers(widget, ['Byte array (hex)', 'Number (hex)', 'Number (dec)', 'Width (bits)'])

        self.ui.highlight_deps_checkbox.stateChanged.connect(self._highlight_deps_checkbox_statechanged)

    def _highlight_deps_checkbox_statechanged(self, state):
        if self.ui.highlight_deps_checkbox.isChecked():
            highlight_dependencies(self.binary_view, self.offset)
        else:
            clear_highlights(self.binary_view, self.function)

    def _show_values_in_ui(self, widget, values):
        vals_stripped = values.strip('[]')
        vals_arr = []

        if vals_stripped != '':
            vals_arr = vals_stripped.split(',')

        items = []

        for val in vals_arr:
            num = little_endian_byte_str_to_num(val)
            items.append([
                '0x' + val,             # Byte array (hex)
                hex(num),               # Number (hex)
                num,                    # Number (dec)
                _byte_str_numbits(val)  # Width (bits)
                ])

        set_items(widget, items)

    def _update_values(self):
        db = get_database()

        addr = self.offset
        cur_file = get_image_name(self.binary_view)

        query = f'''
            MATCH (ins:Instruction {{image_offset: {addr}}})
            WHERE ins.image_name CONTAINS "{cur_file}"
            AND ins.read_values IS NOT NULL
            AND ins.written_values IS NOT NULL
            RETURN ins'''

        results = db.run_list(query)

        read_vals = '[]'
        written_vals = '[]'

        if len(results) > 0:
            read_vals = results[0]['ins']['read_values']
            written_vals = results[0]['ins']['written_values']

        self._show_values_in_ui(self.ui.read_values_tablewidget, read_vals)
        self._show_values_in_ui(self.ui.written_values_tablewidget, written_vals)

    def onOffsetChanged(self):
        if self.ui.highlight_deps_checkbox.isChecked():
            highlight_dependencies(self.binary_view, self.offset)
        else:
            clear_highlights(self.binary_view, self.function)

        self._update_values()
