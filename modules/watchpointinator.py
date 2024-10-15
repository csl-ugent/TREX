# lldb's python interface is shipped with lldb itself
# so we can't install it in the virtual environment.
# Instead, we let the user install it on the host system
# and set the path to the lldb Python module using the
# LLDB_PYTHON_MODULE_PATH environment variable.

import argparse
import os
import sys
import threading

from pathlib import Path
from typing import Tuple, Union, Optional, Callable

if 'LLDB_PYTHON_MODULE_PATH' not in os.environ:
    raise ImportError('You need to set the path to the lldb Python package in the LLDB_PYTHON_MODULE_PATH environment variable (see README.md)')

# -- import lldb --
old_path = sys.path
sys.path.insert(0, os.environ['LLDB_PYTHON_MODULE_PATH'])
import lldb
sys.path = old_path

from core.workspace import Workspace

def get_path_from_pid(pid:int) -> Path:
    symlink_path = Path(f'/proc/{pid}/exe')
    return symlink_path.readlink()

class WatchpointBroker:
    def __init__(self, pid:Union[int,str]) -> None:
        self._pid:int = int(pid)
        self._watchpoints = []
        self._listeners = {None: []}

        self._debugger:lldb.SBDebugger = self._setup_debugger()
        self._target:lldb.SBTarget
        self._process:lldb.SBProcess
        self._target, self._process = self._attach_debugger(self._debugger, self._pid)

    @staticmethod
    def _setup_debugger() -> lldb.SBDebugger:
        debugger = lldb.SBDebugger.Create()

        # Disable async mode for LLDB.
        # This means that when we step or continue, LLDB won't return
        # from the function call until the process stops.
        debugger.SetAsync(False)

        return debugger
    
    @staticmethod
    def _attach_debugger(debugger:lldb.SBDebugger, pid:int) -> Tuple[lldb.SBTarget, lldb.SBProcess]:
        elf_path = get_path_from_pid(pid)

        target = debugger.CreateTarget(str(elf_path))
        error = lldb.SBError()
        process = target.AttachToProcessWithID(debugger.GetListener(), int(pid), error)

        if not error.Success():
            raise RuntimeError(f'Could not attach to process with id {pid}: {error}')

        return (target, process)

    def watch(self, address:int, callback:Optional[Callable]=None) -> lldb.SBWatchpoint:
        error = lldb.SBError()
        wp = self._target.WatchAddress(
            address,
            1,     # size
            False, # read
            True,  # modify
            error  # error
        )

        if not error.Success():
            raise RuntimeError(f'Could not set watchpoint: {error}')
        
        if callback is not None:
            self.add_listener(
                callback = callback,
                watchpoint = wp,
            )

        return wp
    
    def add_listener(self, callback:Callable, watchpoint:Optional[lldb.SBWatchpoint]=None):
        if watchpoint not in self._listeners:
            self._listeners[watchpoint] = []
        
        if watchpoint is not None:
            wp_id = watchpoint.GetID()
            self._listeners[wp_id].append(callback)
        else:
            self._listeners[None].append(callback)
    
    def run(self):
        while self._process.GetState() != lldb.eStateExited:
            self._process.Continue()

            for t in self._process:
                if t.GetStopReason() == lldb.eStopReasonWatchpoint:
                    # This thread triggered the watchpoint
                    triggered_wp_idx = t.GetStopReasonDataAtIndex(0) - 1

                    triggered_wp = self._target.GetWatchpointAtIndex(triggered_wp_idx)

                    frame = t.GetFrameAtIndex(0)

                    self._notify(frame, triggered_wp)

    def _notify(self, frame:lldb.SBFrame, watchpoint:lldb.SBWatchpoint):
        wp_id = watchpoint.GetID()
        for cb in self._listeners.get(wp_id, []) + self._listeners[None]:
            cb(frame, watchpoint)
    
    @property
    def watchpoints(self) -> list[lldb.SBWatchpoint]:
        return list(self._target.watchpoint_iter())

class Watchpointinator(WatchpointBroker):
    def __init__(self, pid:Union[int,str], watchpoints:list[int]=[]) -> None:
        super().__init__(pid)

        self.add_listener(self._callback)
        
        for wp_addr in watchpoints:
            self.watch(wp_addr)
        
        self._known_arcs = set()
    
    @staticmethod
    def _cypher_properties_to_str(d:dict) -> str:
        res = ", ".join([f"{k}: {repr(v)}" for k,v in d.items() if v is not None])
        return f"{{{res}}}"

    def _insert_in_db(self, frame:lldb.SBFrame, mem_loc:int):      
        insn_offset = frame.addr.file_addr
        
        # -- check if we should even bother inserting these in the db --

        if (insn_offset, mem_loc) in self._known_arcs:
            return
        
        self._known_arcs.add((insn_offset, mem_loc))

        # -- extract useful info --
        try:
            (insn,) = self._target.ReadInstructions(frame.addr, 1)
        except ValueError:
            insn = None
        function = frame.GetFunction()

        image_name = self._target.executable.GetFilename()

        routine_offset = None
        if function is not None:
            routine_offset = function.addr.file_addr
        if routine_offset == (1 << 64) - 1:
            routine_offset = None

        _insn_properties = {
            "image_name":     image_name,
            "image_offset":   str(insn_offset),

            "routine_name":   frame.symbol.name,
            "routine_offset": str(routine_offset) if routine_offset is not None else None,
            
            "opcode":         insn.GetMnemonic(self._target) if insn is not None else None,
            "operands":       insn.GetOperands(self._target) if insn is not None else None,
            "bytes":          bytes(insn.GetData(self._target).uint8).hex() if insn is not None else None,
        }

        # -- insert insn and mem loc int db --

        db = Workspace.current.graph
        db.run(f"MERGE (insn:Instruction {self._cypher_properties_to_str(_insn_properties)})")
        db.run(f"MERGE (buf:MemoryBuffer {{start_address: '{mem_loc}'}})")

        # -- link target to memory buffers --

        db.run(f"""
            MATCH(insn:Instruction)
            WHERE insn.image_offset = '{insn_offset}' AND insn.image_name = '{image_name}'
            MATCH (buf:MemoryBuffer)
            WHERE buf.start_address = '{mem_loc}'
            CREATE (insn)-[r:MODIFIES]->(buf)
            RETURN r
        """)

    def _callback(self, frame, wp):
        wp_addr = wp.GetWatchAddress()

        db_thread = threading.Thread(target=self._insert_in_db, args=(frame, wp_addr))
        db_thread.start()

def run_watchpointinator(pid:int, watchpoints:list[int]):
    wpinator = Watchpointinator(pid, watchpoints)
    wpinator.run()

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser()

    arg_parser.add_argument("pid", type=int)
    arg_parser.add_argument("--watchpoints", "-w", nargs='+', type=lambda s:int(s, base=0))

    args = arg_parser.parse_args()

    run_watchpointinator(args.pid, args.watchpoints)