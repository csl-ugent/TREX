import os
from os import path

from mate_attack_framework.neo4j_py2neo_bridge import Graph
from binaryninja import *
from mate_attack_framework.util import get_image_name, get_database, get_function_containing_address

def _get_instruction_textline(function: Function, address: int):
    '''Convert the instruction at the given address to a representation suitable for display in a FlowGraph.'''
    for ins in function.instructions:
        ins_addr = ins[1]

        if ins_addr == address:
            return DisassemblyTextLine(ins[0])

def _show_dataflowgraph(function: Function, instruction_query: str, instruction_query_addr, connection_query: str, connection_query_addr1, connection_query_addr2):
    db = get_database()
    dfg = FlowGraph()

    # At the moment, Binary Ninja's flowgraph only supports single-entry graphs
    # (like CFGs), so we simulate this by adding an empty 'root' node that is a
    # predecessor of all nodes in the graph.
    # TODO: Remove this workaround when Binary Ninja is updated.
    root_node = FlowGraphNode(dfg)
    root_node.lines = []
    dfg.append(root_node)
    root_edge_style = EdgeStyle(EdgePenStyle.DashDotDotLine, 0, ThemeColor.GraphBackgroundDarkColor)

    # First, add the nodes.
    node_map = {}

    for record in db.run_list(instruction_query):
        addr = instruction_query_addr(record)

        node = FlowGraphNode(dfg)
        node.lines = [_get_instruction_textline(function, addr)]
        dfg.append(node)

        node_map[addr] = node

        # TODO: Remove this workaround when Binary Ninja is updated.
        root_node.add_outgoing_edge(BranchType.UserDefinedBranch, node, root_edge_style)

    # Next, add the connections between nodes.
    for record in db.run_list(connection_query):
        addr1 = connection_query_addr1(record)
        addr2 = connection_query_addr2(record)

        node1 = node_map[addr1]
        node2 = node_map[addr2]

        node1.add_outgoing_edge(BranchType.UserDefinedBranch, node2)

    # Show the data flow graph.
    show_graph_report("Data Flow Graph", dfg)

def show_dataflowgraph_for_function(binary_view: BinaryView, function: Function):
    '''Show a graphical representation of the data flow graph for a given function.'''
    cur_file = get_image_name(binary_view)
    addr_lo = function.start
    addr_hi = function.highest_address

    instruction_query = f'''
        MATCH (ins:Instruction)
        WHERE ins.image_name CONTAINS "{cur_file}"
        AND ins.image_offset >= {addr_lo}
        AND ins.image_offset <= {addr_hi}
        RETURN (ins)'''

    instruction_query_addr = lambda record: record['ins']['image_offset']

    connection_query = f'''
        MATCH (ins1:Instruction) <-[:DEPENDS_ON]- (ins2:Instruction)
        WHERE ins1.image_name CONTAINS "{cur_file}"
        AND ins2.image_name CONTAINS "{cur_file}"
        AND ins1.image_offset >= {addr_lo}
        AND ins2.image_offset >= {addr_lo}
        AND ins1.image_offset <= {addr_hi}
        AND ins2.image_offset <= {addr_hi}
        RETURN DISTINCT (ins1), (ins2)'''

    connection_query_addr1 = lambda record: record['ins1']['image_offset']
    connection_query_addr2 = lambda record: record['ins2']['image_offset']

    _show_dataflowgraph(function, instruction_query, instruction_query_addr, connection_query, connection_query_addr1, connection_query_addr2)


def show_dataflowgraph_for_instruction(binary_view: BinaryView, instruction_address: int):
    '''Show a graphical representation of the data flow graph for a given instruction.'''
    cur_file = get_image_name(binary_view)
    function = get_function_containing_address(binary_view, instruction_address)
    addr_lo = function.start
    addr_hi = function.highest_address

    instruction_query = f'''
        MATCH p = (ins1:Instruction) -[:DEPENDS_ON*]-> (ins2:Instruction)
        WHERE ins1.image_name = "{cur_file}"
        AND ins1.image_offset = {instruction_address}
        AND ALL (
            n IN nodes(p)
            WHERE n.image_name = "{cur_file}"
            AND n.image_offset >= {addr_lo}
            AND n.image_offset <= {addr_hi}
        )
        RETURN DISTINCT ins2 AS ins
        UNION ALL
        MATCH (ins1:Instruction)
        WHERE ins1.image_name = "{cur_file}"
        AND ins1.image_offset = {instruction_address}
        RETURN ins1 AS ins
        '''

    instruction_query_addr = lambda record: record['ins']['image_offset']

    connection_query = f'''
        MATCH p = (ins1:Instruction) -[:DEPENDS_ON*]-> (ins2:Instruction)
        WHERE ins1.image_name = "{cur_file}"
        AND ins1.image_offset = {instruction_address}
        AND ALL (
            n IN nodes(p)
            WHERE n.image_name = "{cur_file}"
            AND n.image_offset >= {addr_lo}
            AND n.image_offset <= {addr_hi}
        )
        UNWIND apoc.coll.pairsMin(nodes(p)) AS pair
        RETURN DISTINCT pair'''

    connection_query_addr1 = lambda record: record['pair'][1]['image_offset']
    connection_query_addr2 = lambda record: record['pair'][0]['image_offset']

    _show_dataflowgraph(function, instruction_query, instruction_query_addr, connection_query, connection_query_addr1, connection_query_addr2)
