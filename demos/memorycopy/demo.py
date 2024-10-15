import inspect
import os

from core.workspace import Workspace
from modules.generic_deobfuscator import GenericDeobfuscator
from modules.memory_buffers import MemoryBuffersModule
from modules.data_dependencies import DataDependenciesModule
from modules.basic_block_profiler import BasicBlockProfilerModule
import binaryninja
from binaryninja import PluginCommand


def demo():
    filename = inspect.getframeinfo(inspect.currentframe()).filename
    path = os.path.dirname(os.path.abspath(filename))
    demo_file = os.path.join(path, 'memorycopy.exe')
    Workspace.create_new('memorycopy', demo_file)
    Workspace.select('memorycopy')

    # Set binary parameters
    binary_params = ""
    recorder = Workspace.current.create_recorder(binary_params)
    recorder.run()

    # Apply Memory Buffer Pin plugin
    memory_buffers = MemoryBuffersModule(binary_params, 30, recorder)
    memory_buffers.run()

    # Apply Data Dependencies Pin plugin
    data_deps = DataDependenciesModule(binary_params, 30, recorder=recorder)
    data_deps.run()

    # Apply Basic Block Profiler Pin plugin
    bbl_profiler = BasicBlockProfilerModule(binary_params, 30, recorder=recorder)
    bbl_profiler.run()

    # Apply Generic deobfuscator
    generic_deobfuscator = GenericDeobfuscator(binary_params, 30, recorder)
    generic_deobfuscator.run()

    # Create BinaryNinja DB with VSA applied.
    os.environ['MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS'] = '1'

    # For debugging only:
    # binaryninja.log.log_to_file(binaryninja.LogLevel.DebugLog, '/tmp/bn.log', append=False)

    with binaryninja.load(demo_file) as bv:
        print(f'Opened file {demo_file} in BinaryNinja.')

        print('Applying VSA to loads...')
        ctx = binaryninja.PluginCommandContext(bv)
        cmd = PluginCommand.get_valid_list(ctx)['MATE Attack Framework\\Memory buffers\\Apply VSA to loads']
        cmd.execute(ctx)

        db_path = os.path.join(path, 'memorycopy.bndb')
        saved = bv.create_database(db_path)
        assert saved, 'Could not save BN database'

        print(f'Saved BinaryNinja database to {db_path}')
