from pathlib import Path
import shutil
import subprocess

from core.workspace import Workspace
from modules.scanmem.scanmem import run_scanmem_wrapper
from modules.watchpointinator import run_watchpointinator

from time import sleep

def demo():
    folder_path = Path(__file__).parent
    demo_file = folder_path / 'supertux' / 'build' / 'supertux2'
    Workspace.create_new('supertux', demo_file)
    Workspace.select('supertux')

    # -- Launch supertux --
    # as long as we do not call .communicate on the Popen instance, this will
    # run this in a non-blocking manner (https://stackoverflow.com/a/7224186)
    supertux_proc = subprocess.Popen([str(demo_file)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # -- Run scanmem --
    mp_name = "coins"
    run_scanmem_wrapper(supertux_proc.pid, mp_name)

    # -- Run watchpointinator --
    # - Retrieve scanmem results from db -
    db = Workspace.current.graph

    query = f"""
        MATCH (mp:Milepost {{name: '{mp_name}'}})-[r:MIGHT_BE_LOCATED_AT]->(buf:MemoryBuffer)
        RETURN buf
    """

    watchpoint_addresses = [int(buf["buf"]["start_address"]) for buf in db.run_list(query)]

    # - Run the actual thing -
    
    print(f"-- Setting watchpoints at {watchpoint_addresses} --")

    run_watchpointinator(supertux_proc.pid, watchpoint_addresses)
