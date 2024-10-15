import re
import os

from core.workspace import Workspace
from graph_models.disassembly_analysis import *


class DotIO:
    @staticmethod
    def cfg_from_dot(image_name, dotfile):
        disassembly = Disassembly()

        instructions = {}
        edges = {}
        with open(dotfile) as f:
            print("DotIO> parsing file: {}".format(dotfile))
            # skip header
            f.readline()

            for line in f:
                addr_pos = line.find("address")
                if addr_pos > -1:
                    # instruction node: "address" [inst="mnem", address="hex"]
                    address = int(line[addr_pos+9:].split('"')[0], base=16)
                    ins_pos = line.find("inst")
                    ins = line[ins_pos+6:line.find('"', ins_pos+6)].partition(' ')
                    mnem = ins[0]
                    operands = ins[2].split(', ') if ins[2] else []
                    # construct OGM node
                    ins_node = Instruction()
                    ins_node.image_name = os.path.basename(image_name)
                    ins_node.image_offset = address
                    ins_node.mnem = mnem
                    ins_node.operands = operands
                    instructions[address] = ins_node
                    continue

                child_pos = line.find("child_number")
                if child_pos > -1:
                    child_number = int(line[child_pos+13:-2])
                    tmp = line.split('" -> "')
                    source = int(tmp[0][1:], base=16)
                    dest = int(tmp[1].split('"')[0], base=16)
                    if source in edges:
                        edges[source][child_number] = dest
                    else:
                        edges[source] = {child_number: dest}

        print("DotIO> constructing cfg")

        current_block = None
        previous_block = None
        add_edge = False
        for address in sorted(instructions.keys()):
            current_ins = instructions[address]

            if current_ins.basic_block.first():
                # a new block is defined here
                current_block = current_ins.basic_block.first()
            elif current_block:
                # add ins to current block
                current_block.instructions.add(current_ins)
                current_ins.basic_block.add(current_block)
            else:
                # start a new block
                current_block = BasicBlock()
                disassembly.basic_blocks.add(current_block)
                current_block.image_name = current_ins.image_name
                current_block.image_offset_begin = current_ins.image_offset
                current_block.instructions.add(current_ins)
                current_ins.basic_block.add(current_block)

            if previous_block:
                previous_block.image_offset_end = current_ins.image_offset
                if add_edge:
                    previous_block.out_set.add(current_block, type='FALLTHROUGH')
            previous_block = current_block
            if address in edges:
                add_edge = False
                edge = edges[address]
                if 2 in edge:
                    # branch target
                    target_ins = instructions[edge[2]]
                    target_block = target_ins.basic_block.first()
                    if target_block:
                        # define a new basic block at the branch target if not already there
                        if target_block.image_offset_begin != target_ins.image_offset or target_block.image_name != target_ins.image_name:
                            new_block = BasicBlock()
                            new_block.image_name = target_ins.image_name
                            new_block.image_offset_begin = target_ins.image_offset
                            disassembly.basic_blocks.add(new_block)
                            # split instructions in old block over new block
                            target_block_ins = target_block.instructions._related_objects.copy()
                            target_block.instructions.clear()
                            for ins, _ in target_block_ins:
                                if ins.image_offset >= new_block.image_offset_begin:
                                    new_block.instructions.add(ins)
                                    ins.basic_block.remove(target_block)
                                    ins.basic_block.add(new_block)
                                else:
                                    target_block.instructions.add(ins)
                            # set new block image_offset_end
                            new_block.image_offset_end = target_block.image_offset_end
                            # move edges from old block to new block
                            new_block.out_set.__related_objects = target_block.out_set._related_objects.copy()
                            target_block.out_set.clear()
                            # add fallthrough edge between old block and new block
                            target_block.out_set.add(new_block, type='FALLTHROUGH')
                            target_block.image_offset_end = target_ins.image_offset
                            target_block = new_block
                    else:
                        target_block = BasicBlock()
                        target_block.image_name = target_ins.image_name
                        target_block.image_offset_begin = target_ins.image_offset
                        target_block.instructions.add(target_ins)
                        target_ins.basic_block.add(target_block)
                        disassembly.basic_blocks.add(target_block)
                    # add jump edge between current block and target block
                    current_block = current_ins.basic_block.first()
                    current_block.out_set.add(target_block, type='JUMP')
                    previous_block = current_block
                    current_block = None

                    if 1 in edge:
                        # start new block, with fallthrough edge from previous
                        add_edge = False

            else:
                # no edge or fallthrough: ret or dynamic jmp
                # close current_block
                current_block = None

        print("DotIO> imported {} instructions divided over {} basic blocks".format(
            len(instructions), len(disassembly.basic_blocks._related_objects)))
        return disassembly

    @staticmethod
    def cfg_to_dot(disassembly, dotfile):
        node_template = '"{}" [inst="{}", address="{}"]\n'
        edge_template = '"{}" -> "{}" [child_number={}]\n'
        grap_nodes = []
        grap_edges = []

        blocks = None
        instructions = None
        if disassembly.__primaryvalue__:
            ins_query = """
                        MATCH (d:Disassembly) WHERE ID(d)=$analysis_id WITH d
                        MATCH (i:Instruction)<-[:HAS_INSTRUCTION]-(b:BasicBlock)<-[:DEFINES_BLOCK]-(d)
                        RETURN i AS node, ID(b) AS block_id ORDER BY i.image_offset
                        """
            instructions = Workspace.current.graph.run(ins_query, {'analysis_id': disassembly.__primaryvalue__})
            edge_query = """
                         MATCH (d:Disassembly) WHERE ID(d)=$analysis_id WITH d
                         MATCH (t:BasicBlock)<-[edge:HAS_CF_EDGE]-(s:BasicBlock)<-[:DEFINES_BLOCK]-(d)
                         MATCH (s)-[:HAS_INSTRUCTION]->(i:Instruction)
                         RETURN edge.type AS type, t.image_offset_begin AS target, MAX(i.image_offset) AS source
                         """
            edges = Workspace.current.graph.run(edge_query, {'analysis_id': disassembly.__primaryvalue__})

            """
            query = "MATCH (b:BasicBlock)<-[:DEFINES_BLOCK]-(d:Disassembly) WHERE ID(d)=$analysis_id RETURN b"
            cursor = Workspace.current.graph.run(query, { 'analysis_id': disassembly.__primaryvalue__ })
            blocks = [ BasicBlock.wrap(block_node) for block_node in cursor.to_series() ]
            """
        else:
            blocks = [rel[0] for rel in disassembly.basic_blocks._related_objects]

        with open(dotfile, 'w') as f:
            f.write('digraph G {\n')

            current_block = 0
            previous_address = None
            while instructions.forward():
                ins = instructions.current
                address = hex(ins['node']['image_offset'])
                f_address = format(ins['node']['image_offset'], 'x')
                mnem = ins['node']['mnem']
                operands = ins['node']['operands'] or []
                operands = [re.sub(r'\b([0-9A-Fa-f]*h)\b', lambda num: '0x'+num.group(1)[:-1], op) for op in operands]
                operands = " " + ', '.join(operands)
                grap_nodes.append(node_template.format(f_address, mnem + operands, address))

                if current_block == ins['block_id'] and previous_address:
                    grap_edges.append(edge_template.format(previous_address, f_address, 1))
                current_block = ins['block_id']
                previous_address = f_address

            while edges.forward():
                edge = edges.current
                source_address = format(edge['source'], 'x')
                destination_address = format(edge['target'], 'x')
                child_number = 1 if edge['type'] == 'FALLTHROUGH' else 2
                grap_edges.append(edge_template.format(source_address, destination_address, child_number))

            for node in grap_nodes:
                f.write(node)
            for edge in grap_edges:
                f.write(edge)
            f.write('}')
