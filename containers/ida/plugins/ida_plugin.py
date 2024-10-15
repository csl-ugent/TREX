import getopt
import os
import re
import sys
import json

from Tool import EDGE_FALLTHROUGH, EDGE_JUMP
from idapython import InsAssembled, InsString, InsIsHanging, InsIsData, InsIsUnknown, iter_blocks, BblOutgoingInsns, \
    InsNext, InsIsFlow, FunctionAppendChunk, BblIncomingInsns, InsDefinesPC, InsIsBranch, InsIsCall, iter_all_insns, \
    InsSwitchInfo, SwitchNrCases, Dechunkize, iter_insns, iter_blocks_in_function, iter_hanging, InsIsLastInBbl, \
    InsNotInTextSection, InsIsConditional, args, find_section, analysed_binary

from idaapi import get_func
from idautils import DecodeInstruction, Functions, Strings, DataRefsTo
from idc import prev_head, next_head, print_operand, print_insn_mnem, get_func_name, auto_wait, qexit

# ~/idapro-7.5/ida64 -a- -A -Lida.log -S$PWD/containers/ida/plugins/ida_plugin.py <binary path>
# ./idaq -c -A -S"/home/jens/repositories/ida-plugins/test.py [<argument>]*" <binary path and name>
#
# Generate a list of instructions, including "hanging instructions"

INVALID = -1

# debug
debug = True
stop = False

# output files
ifile = None
ffile = None
bfile = None
cfile = None
efile = None
stringfile = None
stringxreffile = None

# global statistics
nr_hanging = 0
function_id = 0
block_id = 0
edge_id = 0

# global variables
plt_start = 0
plt_end = 0
color_codes_re = re.compile(r'\x1b[^m]*m')

# arguments
assoc_hanging_actions = ''
assoc_hanging_jump = False
switch_dest_to_tail = False
suffix = ''


def AddressInPLT(address):
    return plt_start <= address < plt_end


def AddressInRange(S, E, address):
    return S <= address < E


edge_types = ['FALLTHROUGH', 'JUMP', 'CALL', 'CALL_FALLTHROUGH']


def print_edge(src, branch, dst, etype, decoded):
    global edge_id

    if etype == EDGE_FALLTHROUGH:
        delta = dst - branch
        if delta > 4:
            # CHANGED assert decoded.is_macro(), "0x%x (0x%x) -> 0x%x (type %d) expected delta of 8, got %d" % (src, branch, dst, etype, delta)
            # here, IDA has probably merged two instructions, for example MOVW/MOVT
            branch += decoded.size - 4
    etype = edge_types[etype]
    if not AddressInPLT(dst):
        efile.write("%d,%d,%d,%d,%d,%s\n" % (edge_id, src, branch, dst, 0, etype))

    edge_id += 1


def dangerous_flow_instruction(i):
    result = False

    asm = InsAssembled(i)

    # Certain instruction cause IDA to infinitely loop in its analysis phase.
    # This is the case in cactusADM (AF/swo, max4, address 0xecdc): LDMEQFD SP!, {R4, PC}
    if (asm & 0xffff8000) == 0x08bd8000:
        print("dangerous instruction 0x%x %s" % (i, InsString(i)))
        result = True

    return result


def put_in_bbl(i):
    return InsIsHanging(i) and not InsIsData(i) and not InsIsUnknown(i)


def associate_hanging_to_head(S, E):
    global stop
    result = False
    blocks = set()

    print("    hanging TAIL instructions to HEAD function...")

    #####
    # head -> tail
    # if the head is part of a function, and the tail is HANGING, add the tail
    # to that function if it is connected through a fallthrough edge

    for block in iter_blocks(S, E):
        if InsIsHanging(block.start_ea):
            continue

        # outgoing edges
        block_end = prev_head(block.end_ea)
        for i in BblOutgoingInsns(block_end):
            if not put_in_bbl(i):
                continue

            # at least one outgoing path is HANGING
            # print("block end at 0x%x, for 0x%x" % (block_end, i))
            blocks.add(block_end)
            break

    print("    initial %d blocks" % (len(blocks)))
    while len(blocks) > 0:
        addr = blocks.pop()

        # determine FT path
        decoded = DecodeInstruction(addr)
        if decoded is None:
            print("error, fallthrough decoded instruction not available 0x%x %s" % (addr, InsString(addr)))
            continue

        ft_insn = InsNext(decoded)

        for i in BblOutgoingInsns(addr):
            # only look at FT path because this surely should be put in the same function
            if i != ft_insn and not assoc_hanging_jump:
                continue
            if not put_in_bbl(i):
                continue

            block_start = i
            block_end = i
            # print("starting at 0x%x %d %d %d" % (i, idapython.InsIsHanging(i), idapython.InsIsData(i), idapython.InsIsUnknown(i)))
            # if i == 0xecd8:
            #     stop = True
            #     return False

            # find block end
            increment_end_address = True
            while True:
                end_decoded = DecodeInstruction(block_end)
                if end_decoded is None:
                    print("error, decoded instruction not available 0x%x %s" % (block_end, InsString(block_end)))
                    increment_end_address = False
                    break

                # this is a flow instruction, stop looking for block end here
                is_flow, _, _ = InsIsFlow(end_decoded)
                if is_flow and not dangerous_flow_instruction(block_end):
                    break

                outgoing = BblOutgoingInsns(block_end)
                if len(outgoing) == 0:
                    # e.g., UND #0xb7 followed by UNKNOWN bytes and has 0 outgoing instructions
                    break
                assert len(outgoing) == 1, "multiple outgoing from 0x%x %s" % (block_end, outgoing)
                dst = outgoing[0]

                if (block_end + end_decoded.size) != dst:
                    assert end_decoded.is_macro(),\
                        "end 0x%x, size %d, outgoing 0x%x" % (block_end, end_decoded.size, dst)
                    dst = block_end + end_decoded.size

                if not put_in_bbl(dst):
                    break

                block_end = dst

            # [start, end) for FunctionAppendChunk
            block_end_excl = block_end
            if increment_end_address:
                block_end_excl += DecodeInstruction(block_end).size

            if block_start == block_end_excl:
                # don't do anything if we have made an empty block
                continue

            # print("found block 0x%x to 0x%x-0x%x %d" % (addr, block_start, block_end, idapython.InsIsHanging(block_start)))
            FunctionAppendChunk(addr, block_start, block_end_excl)
            result = True
            blocks.add(block_end)

    return result


def associate_hanging_to_tail(S, E):
    result = False
    blocks = set()

    print("    hanging HEAD instructions to TAIL function...")

    #####
    # tail -> head
    # if the tail is part of a function, and the head is HANGING, add the head
    # block to the tail function

    # initial list of blocks that have at least 1 incoming 'hanging' edge
    for block in iter_blocks(S, E):
        if InsIsHanging(block.start_ea):
            continue

        for i in BblIncomingInsns(block.start_ea):
            if not InsIsHanging(i):
                continue

            # print("initial hanging block 0x%x" % (block.start_ea))
            blocks.add(block.start_ea)
            break

    print("    initial %d blocks" % (len(blocks)))
    while len(blocks) > 0:
        bbl_address = blocks.pop()

        for i in BblIncomingInsns(bbl_address):
            if not InsIsHanging(i):
                continue

            # block boundaries, IDA does not include the last address: [x, y)
            block_start = i

            i_decoded = DecodeInstruction(i)
            block_end = i + i_decoded.size

            # find block start
            while True:
                incoming_insns = BblIncomingInsns(block_start)

                # beginning of block?
                if len(incoming_insns) == 0:
                    break
                if len(incoming_insns) > 1:
                    break

                incoming = incoming_insns[0]
                if not InsIsHanging(incoming):
                    break

                decoded = DecodeInstruction(incoming)

                if InsIsBranch(decoded) or InsDefinesPC(decoded):
                    break
                if InsIsCall(decoded) and not (decoded.ea + decoded.size == block_start):
                    break

                block_start = incoming

            # print("found block 0x%x-0x%x to 0x%x" % (block_start, block_end, bbl_address))
            FunctionAppendChunk(bbl_address, block_start, block_end)
            result = True
            blocks.add(block_start)

    return result


def contains_colour_code(op):
    return color_codes_re.match(op)


def process_segment(S, E):
    global nr_hanging, function_id, block_id, edge_id

    #####
    # construct BBLs of hanging instructions and add them to existing functions
    if len(assoc_hanging_actions) > 0:
        print("associate hanging instructions...")

        it = 0
        keep_going = True
        while keep_going:
            keep_going = False
            # if it == 10:
            #      break

            print("  iteration %d" % it)

            # execute specified actions
            for action in assoc_hanging_actions:
                if action == 'h':
                    keep_going |= associate_hanging_to_head(S, E)
                elif action == 't':
                    keep_going |= associate_hanging_to_tail(S, E)

                if stop:
                    break

            if stop:
                break
            it += 1

    #####
    # move function chunks around if needed
    if switch_dest_to_tail:
        print("moving switch destinations to tail function...")

        # first construct basic blocks begin->end
        bbl_info = {}
        for block in iter_blocks(S, E):
            bbl_info[block.start_ea] = prev_head(block.end_ea)

        print("  recording move actions...")
        recorded_moves = []
        recorded_moves_statistics = {
            'case_to_multiple_functions': 0,
            'case_ends_in_call': 0
        }
        for insn in iter_all_insns(S, E):
            is_switch, switch_info, switch_cases = InsSwitchInfo(insn)

            # only look at switch instructions
            if not is_switch:
                continue

            switch_fun = get_func(insn)
            assert switch_fun is not None

            for range_index in range(SwitchNrCases(switch_info)):
                current_range = switch_cases.cases[range_index]
                assert len(current_range) == 1
                case_target = switch_cases.targets[range_index]

                if InsIsHanging(case_target):
                    print("switch 0x%x target 0x%x hangs" % (insn, case_target))
                    continue
                assert not InsIsHanging(case_target), "target hangs 0x%x" % case_target

                last_ins = bbl_info[case_target]
                if InsIsCall(DecodeInstruction(last_ins)):
                    print("switch 0x%x, target 0x%x ends in call" % (insn, case_target))
                    continue

                outgoing_functions = set()
                for dst in BblOutgoingInsns(last_ins):
                    outgoing_fun = get_func(dst)
                    if outgoing_fun is None:
                        print("no outgoing function for 0x%x" % dst)
                        continue
                    assert outgoing_fun is not None, "no function for 0x%x" % dst

                    if outgoing_fun != switch_fun:
                        # only look at interprocedural edges
                        outgoing_functions.add(outgoing_fun.start_ea)

                if len(outgoing_functions) > 1:
                    # print("switch 0x%x, target 0x%x has multiple outgoing functions, last instruction 0x%x: %s" % (insn, case_target, last_ins, outgoing_functions))
                    recorded_moves_statistics['case_to_multiple_functions'] += 1

                elif len(outgoing_functions) == 0:
                    # print("switch 0x%x, target 0x%x has no outgoing functions" % (insn, case_target))
                    recorded_moves_statistics['case_ends_in_call'] += 1

                else:
                    # move the target
                    recorded_moves.append({
                        'start': case_target,
                        'end': next_head(last_ins),
                        'dst': list(outgoing_functions)[0]
                    })

        print("  move action results: %s" % recorded_moves_statistics)

        # apply chunk moves
        print("executing %d move actions..." % (len(recorded_moves)))
        for move_command in recorded_moves:
            start = move_command['start']
            end_ex = move_command['end']
            function_addr = move_command['dst']

            Dechunkize(start, end_ex)
            FunctionAppendChunk(function_addr, start, end_ex)

    #####
    # still hanging instructions should be put in a list
    print("listing hanging instructions...")
    for insn in iter_hanging(S, E):
        # ifile.write("0x%x:%d:0x%x\n" % (insn, INVALID, 0xffffffff))
        nr_hanging += 1

    # hanging instructions belong to a special function
    # ffile.write("%d,(null)\n" % (INVALID))

    #####
    # process the CFG and emit a list of functions, bbls and instructions
    print("processing instructions...")
    for f in Functions(S, E):
        for block in iter_blocks_in_function(f):
            block_nins = 0

            #####
            # instructions
            # 'block.start_ea' is the address of the first instruction in the BBL
            # 'block.end_ea' is the address of the first instruction AFTER the BBL (last address + 4)
            for insn in iter_insns(block):
                if AddressInPLT(insn):
                    continue
                if not AddressInRange(S, E, insn):
                    continue

                # decode the instruction and construct sets of operand registers
                decoded_insn = DecodeInstruction(insn)
                if not decoded_insn:
                    print("error, no decoded instruction 0x%x in block 0x%x-0x%x" % (insn, block.start_ea, block.end_ea))
                    continue

                # print the instruction
                assem = InsAssembled(insn)
                operands = []
                k = 0
                op = print_operand(insn, k)
                while op:
                    assert not contains_colour_code(op)
                    operands.append(op)
                    k = k + 1
                    op = print_operand(insn, k)
                # dis = idapython.InsString(insn).split()
                mnem = print_insn_mnem(insn)
                ops = ';'.join(operands)
                last = 'TRUE' if InsIsLastInBbl(insn, block) else 'FALSE'
                ifile.write("%d,%d,%s,0x%s,%s,%s\n" % (insn, block_id, last, assem, mnem, ops))
                block_nins += 1

                # we want to consider _all_ possible branch instructions
                is_flow, is_call, is_branch = InsIsFlow(decoded_insn)
                if is_flow:
                    if not InsIsLastInBbl(insn, block):
                        """
                        if D.IsLoaded() and D.InsIsDispatcher(insn):
                            pass

                        elif idapython.InsIsCall(decoded_insn):
                            # calls are commonly placed in the middle of basic blocks by IDA
                            pass

                        elif D.IsLoaded() and D.InsIsOriginal(insn):
                            # sometimes an original switch instruction is not recognised by IDA as such
                            print("vanilla flow instruction not at end of BBL 0x%x-0x%x and is not call 0x%x %s, probably an unrecognised switch" % (block.startEA, block.endEA, insn, idapython.InsString(insn)))

                        elif D.IsLoaded() and D.InsIsData(insn):
                            print("DATA at 0x%x marked as flow instruction not at end of BBL 0x%x-0x%x %s" % (insn, block.startEA, block.endEA, idapython.InsString(insn)))

                        else:
                            # something is very wrong...
                            assert False, "flow instruction not at end of BBL 0x%x-0x%x and is not call 0x%x %s" % (block.startEA, block.endEA, insn, idapython.InsString(insn))
                        """
                        pass
                    #####
                    # outgoing edges

                    # take special care for conditional branches that have the fallthrough case identical to the jump case
                    drew_fallthrough = False
                    for dst in BblOutgoingInsns(insn):
                        # check destination section
                        dst_not_text, section_name = InsNotInTextSection(insn)
                        if dst_not_text:
                            print("0x%x -> 0x%x goes to section %s" % (insn, dst, section_name))
                            continue

                        gone = (dst != (insn + decoded_insn.size))

                        edge_type = EDGE_FALLTHROUGH
                        if is_call:
                            if gone:
                                src_fun_name = get_func_name(insn)
                                dst_name = get_func_name(dst)
                                plt = 'TRUE' if AddressInPLT(dst) else 'FALSE'
                                cfile.write("%d,%s,%d,%s,%s\n" % (insn, src_fun_name, dst, dst_name, plt))
                                # print_edge(block_id, insn, dst, edge_type, decoded_insn)

                        elif is_branch:
                            # print("JUMP 0x%x -> 0x%x, gone? %d" % (insn, dst, gone))
                            if InsIsConditional(decoded_insn):
                                edge_type = EDGE_JUMP if (gone or drew_fallthrough) else EDGE_FALLTHROUGH
                            else:
                                edge_type = EDGE_JUMP
                            print_edge(block_id, insn, dst, edge_type, decoded_insn)

                        else:
                            edge_type = EDGE_FALLTHROUGH
                            print_edge(block_id, insn, dst, edge_type, decoded_insn)

                        if edge_type == EDGE_FALLTHROUGH:
                            drew_fallthrough = True

                elif InsIsLastInBbl(insn, block):
                    # TODO: we can have 0 outgoing edges here, e.g. in the case where the instruction following
                    #       a switch instruction, which should be data (offset-based switch) is marked as code by IDA.
                    #       As such, IDA will try to draw FT edges from this DATA instruction to the next DATA.
                    done = False
                    for i in BblOutgoingInsns(insn):
                        # CHANGED assert not done, "0x%x" % (insn)
                        print_edge(block_id, insn, i, EDGE_FALLTHROUGH, decoded_insn)
                        done = True

            bfile.write("%d,%d,%d,%d\n" % (block_id, block.start_ea, block.end_ea, function_id))
            block_id += 1

        ffile.write("%d,%s\n" % (function_id, get_func_name(f).replace(":", "_")))
        function_id += 1


def parse_args(args):
    global assoc_hanging_actions, assoc_hanging_jump, switch_dest_to_tail, suffix

    try:
        opts, _ = getopt.getopt(args, "h:jsS:", ["hanging", "hanging-follow-jumps", "switch-dest-to-tail", "suffix"])
    except getopt.GetoptError as err:
        print("ERROR:", err)
        sys.exit(1)

    for opt, arg in opts:
        print("doing option %s with value %s" % (opt, arg))
        if opt in ("-h", "--hanging"):
            assoc_hanging_actions = arg
        elif opt in ("-j", "--hanging-follow-jumps"):
            assoc_hanging_jump = True
        elif opt in ("-s", "--switch-dest-to-tail"):
            switch_dest_to_tail = True
        elif opt in ("-S", "--suffix"):
            print("  suffix: %s" % arg)
            suffix = "." + arg


# create a list of instructions based on IDA's analysis results
def run_ida():
    global ifile, ffile, bfile, cfile, efile, stringfile, stringxreffile
    print("[SCRIPT] Waiting for ida to finish auto-analysis..")
    auto_wait()
    print("[SCRIPT] .. resuming script execution")
    parse_args(args()[1:])

    input_file = analysed_binary()
    basename = input_file + ".ida" + suffix

    # output files
    ifile = open(basename + ".instructions", "w")
    ifile.write("address,block_id,last,assembly,mnem,operands\n")
    print("Writing instructions to %s" % os.path.realpath(ifile.name))

    ffile = open(basename + ".functions", "w")
    ffile.write("id,name\n")
    print("Writing functions to %s" % os.path.realpath(ffile.name))

    bfile = open(basename + ".bbls", "w")
    bfile.write("id,startEA,endEA,function_id\n")
    print("Writing BBLs to %s" % os.path.realpath(bfile.name))

    cfile = open(basename + ".calls", "w")
    cfile.write("ins_address,ins_function_name,dest_address,dest_name,plt\n")
    print("Writing calls to %s" % os.path.realpath(cfile.name))

    efile = open(basename + ".edges", "w")
    efile.write("id,block_id,ins_address,dest_address,plt,edge_type\n")
    print("Writing edges to %s" % os.path.realpath(efile.name))

    stringfile = open(basename + ".strings", "w")
    stringfile.write("address,length,strtype,string\n")
    print("Writing strings to %s" % os.path.realpath(stringfile.name))

    stringxreffile = open(basename + ".stringxrefs", "w")
    stringxreffile.write("string_address,xref_addr\n")
    print("Writing string xrefs to %s" % os.path.realpath(stringxreffile.name))

    # statically linked binaries don't have a PLT section
    global plt_start, plt_end
    plt_start, plt_end = find_section(".plt")
    plt_got_start, plt_got_end = find_section(".plt.got")
    if plt_got_start != 0 and plt_got_end != 0:
        if plt_end == plt_got_start:
            plt_end = plt_got_end

    # we only look at the contents of the .text section
    S, E = find_section(".text")
    if S > 0:
        process_segment(S, E)

    S, E = find_section(".init")
    if S > 0:
        process_segment(S, E)

    # List all strings.
    for s in Strings():
        stringfile.write("%d,%d,%d,%s\n" % (s.ea, s.length, s.strtype, json.dumps(str(s))))

        # List all references to strings.
        for ref in DataRefsTo(s.ea):
            stringxreffile.write("%d,%d\n" % (s.ea, ref))

    ifile.close()
    ffile.close()
    bfile.close()
    efile.close()
    stringfile.close()
    stringxreffile.close()

    # make sure IDA exits, and does not show a GUI
    qexit(0)


def PLUGIN_ENTRY():
    print("plug entry")
    run_ida()


# when this script is loaded as the main program (i.e., a plugin in IDA),
# execute the 'run_ida' function
if __name__ == "__main__":
    print("running as plugin")
    run_ida()
else:
    run_ida()
