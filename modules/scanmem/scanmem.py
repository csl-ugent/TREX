#!/usr/bin/env python3

from time import sleep
from typing import Optional, Union, Any, Callable
import sys
import argparse
import shlex
import subprocess
import threading
import re

from core.workspace import Workspace

def run_command(tool:str, args:str) -> str:
    """Helper function to run commands

    Args:
        tool (str): The name of the command
        args (str): The arguments of the command

    Returns:
        str: The stdout output of the command
    """    
    args = shlex.split(args)

    p = subprocess.run([tool, *args], capture_output=True)

    return str(p.stdout.decode())

def get_pid(name:str) -> str:
    res = run_command("pgrep", name)
    res = res.splitlines(keepends=False)

    if len(res) == 0:
        raise ValueError(f"Could not find any process with a name that resembles '{name}'")
    
    if len(res) > 1:
        raise ValueError(f"Found multiple candidate processes with names that resemble '{name}'")
    
    return res[0].strip()

# interact with a process: https://stackoverflow.com/a/52544846
class InteractiveProcess:
    def __init__(self, tool:str, show=("stdout", "stderr")) -> None:
        self._tool:str = tool
        self._process:Union[subprocess.Popen, None] = None
        self._listeners = {
            "launch":  [],
            "exit": [],
            "stdout": [],
            "stderr": [],
        }
        self._show = show
        self._exited:bool = False

    def _launch_process(self, args:list[str]) -> subprocess.Popen:
        return subprocess.Popen([self._tool, *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE, text=True)

    def run(self, args:Optional[str]=None):
        if args is None:
            args = []
        else:
            args = shlex.split(args)
        
        self._process = self._launch_process(args)
        self._notify_listeners("launch")

        self._exit_thread = threading.Thread(target=self._handle_exit, args=tuple())
        self._exit_thread.start()

        # output and input on different thread: https://stackoverflow.com/a/53312631
        self._stderr_thread = threading.Thread(target=self._handle_stderr, args=tuple())
        self._stderr_thread.start()

        self._stdout_thread = threading.Thread(target=self._handle_stdout, args=tuple())
        self._stdout_thread.start()
    
    def write(self, s:str):
        self._process.stdin.write(s)
        self._process.stdin.flush()

    def _handle_stdout(self):
        current_line = ""

        while not self._exited:
            current_c = self._process.stdout.read(1)
            current_line += current_c

            if current_c == "\n":
                self._notify_listeners("stdout", current_line)
                current_line = ""

            if "stdout" in self._show:
                print(current_c, end="", flush=True)
    
    def _handle_stderr(self):
        current_line = ""

        while not self._exited:
            current_c = self._process.stderr.read(1)
            current_line += current_c

            if current_c == "\n":
                self._notify_listeners("stderr", current_line)
                current_line = ""

            if "stderr" in self._show:
                print(current_c, end="", flush=True, file=sys.stderr)
    
    def _handle_exit(self):
        self._process.wait()
        self._exited = True

        exit_code = self._process.returncode

        self._notify_listeners("exit", exit_code)

    def add_listener(self, event_type:str, callback:Callable[[str], Any]):
        if event_type not in self._listeners:
            raise ValueError(f"Unknown event_type '{event_type}'. Allowed types are {', '.join(repr(k) for k in self._listeners)}")
        
        self._listeners[event_type].append(callback)
    
    def _notify_listeners(self, event_type:str, event:Optional[Any]=None):
        for callback in self._listeners[event_type]:
            if event is not None:
                callback(event)
            else:
                callback()

    def wait(self):
        self._exit_thread.join()

class IPAgent:
    def __init__(self, proc:InteractiveProcess) -> None:
        self._bind_to_process(proc)
    
    def _bind_to_process(self, proc:InteractiveProcess):    
        self._proc = proc

        self._proc.add_listener("stdout", self._on_stdout)
        self._proc.add_listener("stderr", self._on_stderr)

    def _write(self, s:str, end:str="\n"):
        self._proc.write(f"{s}{end}")
    
    def _on_stdout(self, s:str):
        pass
    
    def _on_stderr(self, s:str):
        pass

class SemiTransparentAgent(IPAgent):
    """Aside from being able to send input autonomously, this agent also passes
    through all input that comes from the main python process' stdin
    """
    def __init__(self, proc: InteractiveProcess) -> None:
        super().__init__(proc)

        self._proc_exited:bool = False

    def _bind_to_process(self, proc: InteractiveProcess):
        super()._bind_to_process(proc)
    
        self._proc.add_listener("launch", self._on_launch)
        self._proc.add_listener("exit", self._on_exit)

    def _handle_stdin(self):
        while not self._proc_exited:
            current_line = sys.stdin.readline() # blocking
            if self._proc_exited:
                break
            
            self._on_stdin(current_line)
            self._write(current_line, end="")
    
    def _on_launch(self):
        stdin_thread = threading.Thread(target=self._handle_stdin, args=tuple(), daemon=True) # deamon threads do not prevent the main thread (and by extension the python process) to terminate
        stdin_thread.start()
    
    def _on_exit(self, exit_code:int):
        self._proc_exited = True
    
    def _on_stdin(self, s:str):
        pass

class ScanmemAgent(SemiTransparentAgent):
    num_matches_regex = re.compile(r"^info: we currently have (?P<num_matches>\d+) matches.$")
    match_info_regex  = re.compile(r"^\[ ?(?P<match_id>\d+)\] (?P<match_addr>[\da-f]+), +\d+ \+ +[\da-f]+, +\w+, \d+, \[(?:[CSILFN](?:8|16|32|64|128)u? )+\]")

    def __init__(self, proc: InteractiveProcess, target_name:str) -> None:
        super().__init__(proc)

        self._target_name = target_name

        self._matches_history = []
        self._final_matches = []
        self._quitting:bool = False

    def _invoke_quit(self):
        self._on_quit()

        self._write("q")
    
    def _invoke_list(self):
        self._write("list")

    def _on_quit(self):
        self._quitting = True

        self._invoke_list()

        sleep(0.5)
    
    def _on_reset(self):
        self._matches_history = []

    def _on_stderr(self, s: str):
        # -- parse current num matches if that is what is output --
        m = self.num_matches_regex.match(s)

        if m:
            num_matches = int(m.group("num_matches"))
            
            if num_matches == 1:
                self._invoke_quit()
            elif self._matches_history:
                last_num_matches = self._matches_history[-1]
                if last_num_matches == num_matches:
                    self._invoke_quit()
            
            self._matches_history.append(num_matches)
        
    def _on_stdout(self, s: str):
        # -- parse the list of current matches, if relevant --
        if self._quitting:
            m = self.match_info_regex.match(s)

            if m:
                match_addr = int(m.group("match_addr"), base=16)
                self._final_matches.append(match_addr)
        
    def _on_stdin(self, s: str):
        if s.strip() == "q":
            self._on_quit()
        
        if s.strip() == "reset":
            self._on_reset()
    
    def _on_exit(self, exit_code: int):
        super()._on_exit(exit_code)

        num_final_matches = len(self._final_matches)
        num_steps = len(self._matches_history)
        
        self._insert_in_db(self._target_name, self._final_matches, num_steps)

    @staticmethod
    def _insert_in_db(target:str, match_locations:list[int], num_iterations:int):
        db = Workspace.current.graph

        # -- insert milepost and addresses  into DB --

        db.run(f"MERGE (mp:Milepost {{name: '{target}'}})")

        for match_addr in match_locations:
            db.run(f"MERGE (buf:MemoryBuffer {{start_address: '{match_addr}'}})")

        # -- link target to memory buffers --

        match_locations_str = [f"'{match_addr}'" for match_addr in match_locations]
        match_locations_str = ", ".join(match_locations_str)
        match_locations_str = f"[{match_locations_str}]"

        db.run(f"""
            MATCH(mp:Milepost)
            WHERE mp.name = '{target}'
            MATCH (buf:MemoryBuffer)
            WHERE buf.start_address IN {match_locations_str}
            CREATE (mp)-[r:MIGHT_BE_LOCATED_AT {{scanmem_num_iterations: {num_iterations} }}]->(buf)
            RETURN r
        """)

def run_scanmem_wrapper(pid:int, milepost_name:str):
    scanmem = InteractiveProcess("scanmem", show=("stdout",))
    agent = ScanmemAgent(scanmem, target_name=milepost_name)

    scanmem.run(str(pid))
    scanmem.wait()

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser()

    arg_parser.add_argument("process", help="Either a pid or (part of) the name of the process. In the latter case the name must be sufficiently unique so that pgrep can determine a single corresponding pid.")
    arg_parser.add_argument("--milepost", required=True, help="A name for the asset or milepost you are looking for using scanmem")

    args = arg_parser.parse_args()

    try:
        pid = int(args.process, base=0)
    except ValueError:
        pid = get_pid(args.process)

    run_scanmem_wrapper(pid, args.milepost)
