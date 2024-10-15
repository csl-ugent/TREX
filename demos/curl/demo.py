import inspect
import os
import shutil

from core.workspace import Workspace
from modules.memory_buffers import MemoryBuffersModule


def demo():
    filename = inspect.getframeinfo(inspect.currentframe()).filename
    path = os.path.dirname(os.path.abspath(filename))
    demo_file = os.path.join(path, 'curl')
    Workspace.create_new('curl', demo_file)
    Workspace.select('curl')

    # Copy over shared libraries. Otherwise, the executable will not run.
    for file in os.listdir(path):
        if '.so' in file:
            src = os.path.join(path, file)
            dst = os.path.join(Workspace.current.path, file)
            shutil.copyfile(src, dst)

    # Set binary parameters
    binary_params = "https://tools.ietf.org/rfc/rfc5246.txt -o /tmp/rfc5246.txt"

    # Apply Memory Buffer Pin plugin
    memory_buffers = MemoryBuffersModule(binary_params=binary_params, timeout=300)
    memory_buffers.run()
