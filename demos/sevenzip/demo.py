import inspect
import os
import shutil

from core.core import Core
from core.workspace import Workspace
from modules.data_dependencies import DataDependenciesModule
from modules.ida import IDADism
from modules.caballero import Caballero
from modules.generic_deobfuscator import GenericDeobfuscator
from modules.memory_buffers import MemoryBuffersModule
from modules.memorymap import MemoryMap
from modules.syscall_trace import SysCallTraceModule


def demo():
    filename = inspect.getframeinfo(inspect.currentframe()).filename
    path = os.path.dirname(os.path.abspath(filename))
    binary_name = "7za-double-patched"
    demofile = os.path.join(path, binary_name)
    Workspace.create_new('szip', demofile)
    Workspace.select('szip')
    workspace = Workspace.current

    # Copy needed files to workspace folder
    file_to_encrypt = 'gen_deobf_input'
    deobf_csv = '7za-double-patched_calls.csv'

    for file in [file_to_encrypt, deobf_csv]:
        file_path = Core().get_subdirectory('demos', 'sevenzip', file)
        shutil.copyfile(file_path, os.path.join(workspace.path, file))

    # Set binary parameters
    binary_params = f"-mmt1 -psecret a test.7z {file_to_encrypt}"
    extra_command = f"rm -f test.7z && " \
                    f"strace " \
                    f"-qqq -e t=read -e s=none -y -o {binary_name}.strace ./{binary_name} {binary_params}"

    recorder = Workspace.current.create_recorder(binary_params, extra_command=extra_command)
    recorder.run()

    # Map the binary sections
    mm = MemoryMap()
    mm.run()

    # BROKEN

    # # Grap static pattern matcher
    # grap = Grap()
    # grap_pattern_matches = grap.run()
    # workspace.insert_analysis(grap_pattern_matches)

    # # Import disassembly used by Grap (based on Capstone)
    # grap.import_disassembly()

    # # Apply Grap static pattern matches to capstone disassembly
    # cap_disassembly = Disassembly.match(workspace.graph).first()
    # pattern_matches = GrapAnalysis.match(workspace.graph).first()
    # annotations = Grap.apply(cap_disassembly, grap_pattern_matches)
    # workspace.insert_annotations(annotations)

    # Run ida disassembly
    ida = IDADism()
    ida.run()

    # # Run Grap static pattern matcher on ida disassembly
    # ida_disassembly = Disassembly.match(workspace.graph).where("_.module = 'IDADism' ").first()
    # grap = Grap(ida_disassembly)
    # grap_pattern_matches = grap.run()
    # workspace.insert_analysis(grap_pattern_matches)

    # # Create annotations between ida disassembly and pattern matches
    # annotations = Grap.apply(ida_disassembly, grap_pattern_matches)
    # workspace.insert_annotations(annotations)

    # Run caballero dynamic analysis
    caballero = Caballero(ratio=0.3, binary_params=binary_params, timeout=3*60, recorder=recorder)
    caballero.run()

    # # Apply the caballero results to capstone disassembly
    # caballero_analysis = CaballeroAnalysis.match(workspace.graph).first()
    # annotations = Caballero.apply(caballero_analysis, cap_disassembly, match_bytes=False)
    # workspace.insert_annotations(annotations)

    # # Apply the caballero results to ida disassembly
    # annotations = Caballero.apply(caballero_analysis, ida_disassembly)
    # workspace.insert_annotations(annotations)

    # Apply Memory Buffer Pin plugin
    memory_buffers = MemoryBuffersModule(binary_params, 30, recorder)
    memory_buffers.run()

    # Apply Data Dependencies Pin plugin
    data_deps = DataDependenciesModule(binary_params, 30, recorder=recorder)
    data_deps.run()

    # Apply SysCall Trace Pin Plugin
    syscall_trace = SysCallTraceModule(binary_params, 0, recorder=recorder)
    syscall_trace.run()

    # FIXME: automatically parse strace -> csv for generic deobfuscator
    # Hardcoded currently (committed) as 7za-double-patched_calls.csv

    # Apply Generic deobfuscator
    generic_deobfuscator = GenericDeobfuscator(binary_params, 0, recorder, ["-T", deobf_csv])
    generic_deobfuscator.run()

    # BROKEN

    # # Fetch those blocks identified by ida and annotated by both caballero and grap
    # print("IDA blocks annotated by caballero and grap:")
    # query = """
    #         MATCH (:Disassembly {module: 'IDADism'})-[:DEFINES_BLOCK]->(b:BasicBlock)
    #         WHERE EXISTS ( (b)-[:HAS_INSTRUCTION]->(:Instruction)<-[:ANNOTATES]-()<-[*0..2]-(:Grap) )
    #         AND   EXISTS ( (b)<-[:ANNOTATES]-()<-[]-(:Caballero) )
    #         RETURN b ORDER BY b.image_name, b.image_offset_begin
    #         """
    # print(query, "\n")
    # cursor = Workspace.current.graph.run(query)
    # while cursor.forward():
    #     bb = BasicBlock.wrap(cursor.current['b'])
    #     print(bb.image_name, bb.image_offset_begin)
