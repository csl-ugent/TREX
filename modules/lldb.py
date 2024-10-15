# lldb's python interface is shipped with lldb itself
# so we can't install it in the virtual environment.
# Instead, we let the user install it on the host system
# and set the path to the lldb Python module using the
# LLDB_PYTHON_MODULE_PATH environment variable.

import os
import sys

if 'LLDB_PYTHON_MODULE_PATH' not in os.environ:
    raise ImportError('You need to set the path to the lldb Python package in the LLDB_PYTHON_MODULE_PATH environment variable (see README.md)')

old_path = sys.path
sys.path.append(os.environ['LLDB_PYTHON_MODULE_PATH'])
import lldb
sys.path = old_path

from containers.lldb.lldbrunner import LLDBRunner
from core.core import Core

class LLDBDebuggerModule():
    platform = None
    debugger = None

    def __init__(self):
        self.runner = LLDBRunner(Core().docker_client)

    def run(self):
        print("\n--- LLDB Debugger ---")
        self.runner.run()

        # Create debugger
        self.debugger = lldb.SBDebugger.Create()

        # Disable async mode for LLDB.
        # This means that when we step or continue, LLDB won't return
        # from the function call until the process stops.
        self.debugger.SetAsync(False)

        # Connect to remote platform
        self.platform = lldb.SBPlatform('remote-linux')
        connect_options = lldb.SBPlatformConnectOptions('connect://localhost:1234')
        err = self.platform.ConnectRemote(connect_options)

        if not err.Success():
            raise RuntimeError(f'Could not connect to remote platform: {err}')

        self.debugger.SetSelectedPlatform(self.platform)

        # Set working directory
        self.platform.SetWorkingDirectory('/tmp')

    def upload_file(self, source_path: str, dest_path: str):
        # dest_path is relative to LLDB's working directory.
        dest_path = os.path.join(self.platform.GetWorkingDirectory(), dest_path)

        src_spec = lldb.SBFileSpec(source_path)
        dst_spec = lldb.SBFileSpec(dest_path)

        err = self.platform.Put(src_spec, dst_spec)

        if not err.Success():
            raise RuntimeError(f'Could not upload file {source_path} to the LLDB container: {err}')

    def run_shell_command(self, command: str):
        err = self.platform.Run(lldb.SBPlatformShellCommand(command))

        if not err.Success():
            raise RuntimeError(f'Could not run command {command} in LLDB container: {err}')

    def resolve_address(self, target: lldb.SBTarget, image_name: str, image_offset: int) -> lldb.SBAddress:
        filespec = lldb.SBFileSpec(image_name, False)
        module = target.FindModule(filespec)
        address = module.ResolveFileAddress(image_offset)

        return address
