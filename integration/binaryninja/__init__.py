import os

from binaryninja import *

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from binaryninjaui import DockHandler

from mate_attack_framework.dependencies.dataflowgraph import show_dataflowgraph_for_function, show_dataflowgraph_for_instruction
from mate_attack_framework.dependencies.highlight import clear_highlights, highlight_dependencies
from mate_attack_framework.memorybuffers.vsa import integrate_value_set_analysis

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from mate_attack_framework.docks.instructioninfodock import InstructionInfoDock
    from mate_attack_framework.docks.memorybufferinfodock import MemoryBufferInfoDock
    from mate_attack_framework.docks.mbadetectdock import MBADetectDock

from mate_attack_framework.branchprofiler.alwaysnevertaken import integrate_always_never_taken_branches
from mate_attack_framework.instructioninfo.debug_info import add_debug_info_comments

import mate_attack_framework.util

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from PySide6.QtCore import Qt

def switch_database_command(binary_view: BinaryView):
    util.switch_database()

# Register general command to switch database.
PluginCommand.register('MATE Attack Framework\\Select different database...', 'Select a different database', switch_database_command)

# Register dependencies commands.
PluginCommand.register_for_function('MATE Attack Framework\\Dependencies\\Show data flow graph for function', 'Show the data flow graph for each instruction in the current function', show_dataflowgraph_for_function)
PluginCommand.register_for_address('MATE Attack Framework\\Dependencies\\Show data flow graph for instruction', 'Show the data flow graph for all instructions in the current function that are part of the data flow graph of the current instruction', show_dataflowgraph_for_instruction)

# Register memory buffers commands.
PluginCommand.register('MATE Attack Framework\\Memory buffers\\Apply VSA to loads', 'Apply value set analysis to loads', integrate_value_set_analysis)

# Register branch profiler commands.
PluginCommand.register('MATE Attack Framework\\Branch profiler\\Apply always/never taken branches', 'Apply the dynamic always taken/never taken information to the static representation in BinaryNinja', integrate_always_never_taken_branches)

# Register instruction info commands.
PluginCommand.register_for_function('MATE Attack Framework\\Instruction info\\Add debug information comments to current function', 'Add filename, line number, and column number as a comment to all instructions in the current function', add_debug_info_comments)

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    # Register docks.
    dock_handler = DockHandler.getActiveDockHandler()
    parent = dock_handler.parent()

    instruction_info_dock = InstructionInfoDock(parent, 'MATE Attack Framework: Instruction info')
    dock_handler.addDockWidget(instruction_info_dock, Qt.BottomDockWidgetArea, Qt.Horizontal, True, False)

    memory_buffer_dock = MemoryBufferInfoDock(parent, 'MATE Attack Framework: Memory buffer info')
    dock_handler.addDockWidget(memory_buffer_dock, Qt.BottomDockWidgetArea, Qt.Horizontal, True, False)

    mba_detect_dock = MBADetectDock(parent, 'MATE Attack Framework: Detect MBA expressions')
    dock_handler.addDockWidget(mba_detect_dock, Qt.BottomDockWidgetArea, Qt.Horizontal, False, False)
