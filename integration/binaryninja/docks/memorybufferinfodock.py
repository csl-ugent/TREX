import os

from mate_attack_framework.ui.ui_memorybufferinfodock import Ui_MemoryBufferInfoDock
from mate_attack_framework.util import get_function_containing_address, get_image_name, get_crossreference_icon, XrefImageDirection, get_database, get_instruction_string_rep, set_headers, set_items
from mate_attack_framework.docks.dockbase import DockBase

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from binaryninjaui import DockContextHandler, UIActionHandler

from PySide6.QtWidgets import QWidget, QHeaderView, QTableWidgetItem

class MemoryBufferInfoDock(DockBase):
    def __init__(self, parent, name):
        DockBase.__init__(self, parent, name, Ui_MemoryBufferInfoDock())

        set_headers(self.ui.memory_buffers_tablewidget, ['Direction', 'Type', 'Start', 'End', 'Size (bytes)', '#Reads', '#Writes', 'Avg. spat. entr.', 'Avg. temp. entr.'])
        set_headers(self.ui.references_tablewidget, ['Direction', 'Address', 'Function', 'Preview'])

        self.ui.current_instruction_checkbox.stateChanged.connect(self._current_instruction_checkbox_changed)
        self.ui.memory_buffers_tablewidget.itemSelectionChanged.connect(self._memory_buffers_tablewidget_selectionchanged)
        self.ui.references_tablewidget.cellDoubleClicked.connect(self._references_tablewidget_celldoubleclicked)

        self.buffer_ids = {}
        self.instr_addresses = {}

    def _current_instruction_checkbox_changed(self, state):
        self._show_memory_buffers()

    def _memory_buffers_tablewidget_selectionchanged(self):
        self._show_memory_buffer_references()

    def _references_tablewidget_celldoubleclicked(self, row, col):
        addr = self.instr_addresses[row]
        self.binary_view.navigate(self.binary_view.view, addr)

    def _show_memory_buffer_references(self):
        db = get_database()
        cur_file = get_image_name(self.binary_view)

        cur_row = self.ui.memory_buffers_tablewidget.currentRow()
        node_id = self.buffer_ids[cur_row]

        self.instr_addresses.clear()

        items = []

        for (relation, icon, direction_text) in [('READS_FROM', get_crossreference_icon(XrefImageDirection.RIGHT_TO_LEFT), 'Read'), ('WRITES_TO', get_crossreference_icon(XrefImageDirection.LEFT_TO_RIGHT), 'Write')]:
            query = f'''
                MATCH (ins:Instruction) -[:{relation}]-> (mem)
                WHERE ID(mem) = {node_id}
                AND ins.image_name CONTAINS "{cur_file}"
                RETURN DISTINCT ins'''

            for record in db.run_list(query):
                addr = int(record['ins']['image_offset'])
                function = get_function_containing_address(self.binary_view, addr)
                instr = get_instruction_string_rep(function, addr)

                direction_item = QTableWidgetItem(direction_text)
                direction_item.setIcon(icon)

                self.instr_addresses[len(items)] = addr

                items.append([
                    direction_item,             # Direction
                    hex(addr),                  # Address
                    function.name,              # Function
                    instr                       # Preview
                    ])

        set_items(self.ui.references_tablewidget, items)

    def _show_memory_buffers(self):
        db = get_database()
        addr = self.offset
        cur_file = get_image_name(self.binary_view)

        self.buffer_ids.clear()

        items = []

        for (relation, icon, direction_text) in [('READS_FROM', get_crossreference_icon(XrefImageDirection.RIGHT_TO_LEFT), 'Read'), ('WRITES_TO', get_crossreference_icon(XrefImageDirection.LEFT_TO_RIGHT), 'Write')]:
            if self.ui.current_instruction_checkbox.isChecked():
                query = f'''
                    MATCH (ins:Instruction {{image_offset: {addr}}}) -[:{relation}]-> (mem)
                    WHERE ins.image_name CONTAINS "{cur_file}"
                    AND ((mem:MemoryBuffer) OR (mem:MemoryRegion))
                    RETURN DISTINCT (mem)'''
            else:
                query = f'''
                    MATCH (ins:Instruction) -[:{relation}]-> (mem)
                    WHERE ins.image_name CONTAINS "{cur_file}"
                    AND ((mem:MemoryBuffer) OR (mem:MemoryRegion))
                    RETURN DISTINCT (mem)'''

            for record in db.run_list(query):
                direction_item = QTableWidgetItem(direction_text)
                direction_item.setIcon(icon)


                mem_type = 'Buffer' if 'MemoryBuffer' in record['mem'].labels else 'Region'

                avg_spat_entr = float(record['mem'].get('average_spatial_entropy', float('nan')))
                avg_temp_entr = float(record['mem'].get('average_temporal_entropy', float('nan')))
                buf_size = record['mem']['end_address'] - record['mem']['start_address']

                self.buffer_ids[len(items)] = record['mem'].id

                items.append([
                    direction_item,                         # Direction
                    mem_type,                               # Type
                    hex(record['mem']['start_address']),    # Start address
                    hex(record['mem']['end_address']),      # End address
                    buf_size,                               # Size (bytes)
                    record['mem']['num_reads'],             # Number of Reads
                    record['mem']['num_writes'],            # Number of writes
                    avg_spat_entr,                          # Average spatial entropy
                    avg_temp_entr,                          # Average temporal entropy
                    ])

        set_items(self.ui.memory_buffers_tablewidget, items)

    def onOffsetChanged(self):
        self._show_memory_buffers()
