import inspect
import os
import cxxfilt

from core.workspace import Workspace
from modules.instruction_info import InstructionInfoModule
from modules.data_dependencies import DataDependenciesModule


def prompt_yesno(prompt):
    while True:
        reply = input(prompt)

        if reply.lower() == 'y':
            return True
        if reply.lower() == 'n':
            return False


def demo():
    filename = inspect.getframeinfo(inspect.currentframe()).filename
    path = os.path.dirname(os.path.abspath(filename))
    demo_file = os.path.join(path, 'triple-memcpy.exe')
    Workspace.create_new('triplememcpy', demo_file)
    Workspace.select('triplememcpy')

    # Ask if we should use shortcuts.
    shortcuts = prompt_yesno('Use shortcuts for data dependencies? (y/n) ')

    # Set binary parameters
    binary_params = ""

    recorder = Workspace.current.create_recorder(binary_params)
    recorder.run()

    # Apply Data Dependencies Pin plugin
    data_deps = DataDependenciesModule(binary_params=binary_params, timeout=0, shortcuts=shortcuts, recorder=recorder)
    data_deps.run()

    # Apply Instruction Info Pin plugin
    instr_info = InstructionInfoModule(binary_params=binary_params, timeout=0, recorder=recorder)
    instr_info.run()

    db = Workspace.current.graph

    # Create GDS graph
    if db.run_evaluate('''CALL gds.graph.exists('data_dependencies') YIELD exists'''):
        db.run('''CALL gds.graph.drop('data_dependencies')''')

    db.run('''CALL gds.graph.project('data_dependencies', 'Instruction', 'DEPENDS_ON')''')

    # offset of the write instruction ("mov rax, 10")
    write_offset = 1 * 16

    # offset of the read instruction ("mov rax, qword ptr [rsp + 8]")
    read_offset = 6 * 16

    # return id of an instruction based on function + offset in function
    def get_node_id(func, func_off):
        return db.run_evaluate(f'''
            MATCH (ins:Instruction)
            WHERE ins.routine_name = "{func}"
            AND ins.routine_offset = {func_off}
            RETURN id(ins)''')

    for from_func in ['f1', 'f2', 'f3']:
        for to_func in ['f1', 'f2', 'f3']:
            print(f'Looking for paths from "mov rax, qword ptr [rsp + 8]" in {from_func} to "mov rax, 10" in {to_func}...')

            paths = db.run_list(f'''
                    MATCH (source:Instruction {{routine_name: '{from_func}', routine_offset:  {read_offset}}})
                    MATCH (target:Instruction {{routine_name: '{to_func}', routine_offset: {write_offset}}})
                    CALL gds.shortestPath.dijkstra.stream('data_dependencies', {{
                        sourceNode: id(source),
                        targetNode: id(target)
                    }})
                    YIELD index, sourceNode, targetNode, totalCost, nodeIds, costs, path
                    RETURN *
                    ''')

            print(f'Found {len(paths)} path(s).')

            for path in paths:
                nodeIds = path['nodeIds']
                for i, nodeId in enumerate(nodeIds):
                    node = db.run_evaluate(f'''
                                MATCH (n)
                                WHERE id(n) = {nodeId}
                                RETURN (n)
                                ''')

                    print(
                        f'''{i:2}: {node['image_name']}+{hex(node['image_offset']):10}'''
                        f''' {node['opcode']:8} {node['operands']:35}'''
                        f''' {cxxfilt.demangle(node['routine_name']):75}'''
                    )
                print()

            print()
