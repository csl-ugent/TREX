from common import print_debug

from idaapi import get_segm_by_name, find_not_func, BADADDR, get_func, FlowChart, is_data, get_full_flags, is_unknown, \
    CF_CALL, NN_ja, NN_jae, NN_jb, NN_jbe, NN_jc, NN_jcxz, NN_je, NN_jecxz, NN_jg, NN_jge, NN_jl, NN_jle, NN_jna, \
    NN_jnae, NN_jnb, NN_jnbe, NN_jnc, NN_jne, NN_jng, NN_jnge, NN_jnl, NN_jnle, NN_jno, NN_jnp, NN_jns, NN_jnz, NN_jo, \
    NN_jp, NN_jpe, NN_jpo, NN_jrcxz, NN_js, NN_jz, NN_jmp, NN_jmpfi, NN_jmpni, NN_jmpshort, CF_CHG1, get_switch_info, \
    calc_switch_cases, SWI_RESERVED
from idautils import Heads, Functions, CodeRefsFrom, CodeRefsTo
from idc import get_root_filename, ARGV, o_reg, o_displ, o_phrase, o_reglist, o_idpspec0, o_void, o_imm, o_near, \
    o_idpspec4, o_idpspec5, o_mem, o_idpspec2, o_idpspec3, get_bytes, get_item_size, get_sreg, prev_head, \
    get_func_name, get_segm_name, GetDisasm, print_insn_mnem, create_insn, auto_wait, append_func_tail, \
    get_func_attr, FUNCATTR_START, remove_fchunk, get_fchunk_attr, FUNCATTR_END, add_func

REG_PC = 15
NR_GPREGS = 16
CONDITION_AL = 14


def args():
    return ARGV


def analysed_binary():
    return get_root_filename()


def find_section(name):
    sec = get_segm_by_name(name)

    S = 0
    E = 0

    if sec is not None:
        S = sec.start_ea
        E = sec.end_ea
        print("found %s (%x - %x)" % (name, S, E))

    return S, E


# iterators
def iter_hanging(S, E):
    # search downwards (1)
    addr = find_not_func(S, 1)

    nr_hanging = 0
    while (addr != BADADDR) and (addr < E):
        yield addr

        addr = find_not_func(addr, 1)


def iter_insns(B):
    for insn in Heads(B.start_ea, B.end_ea):
        yield insn


def iter_blocks_in_function(f):
    cfg = FlowChart(get_func(f))

    for block in cfg:
        # this check is VERY important!
        # IDA may put empty blocks in a function (empty = start_ea==end_ea),
        # but these should not be considered because they may be associated with another function
        if block.start_ea < block.end_ea:
            yield block


def iter_blocks(S, E):
    for function in Functions(S, E):
        for block in iter_blocks_in_function(function):
            yield block


def iter_all_insns(S, E):
    for block in iter_blocks(S, E):
        for ins in iter_insns(block):
            yield ins


# instruction operands
def operand_registers(operand):
    result = set()

    if operand.type == o_reg:
        # General Register (al,ax,es,ds...).
        reg = operand.reg

        result.add(reg)

        print_debug("o_reg r%d" % reg)

    elif operand.type == o_displ:
        # Memory Reg [Base Reg + Index Reg + Displacement].
        base_reg = operand.phrase
        index_reg = operand.specflag1
        displacement = operand.addr

        result.add(base_reg)
        result.add(index_reg)

        print_debug("o_displ r%d r%d %d" % (base_reg, index_reg, displacement))

    elif operand.type == o_phrase:
        # Memory Ref [Base Reg + Index Reg].
        base_reg = operand.phrase
        index_reg = operand.specflag1

        result.add(base_reg)
        result.add(index_reg)

        print_debug("o_phrase r%d r%d" % (base_reg, index_reg))

    elif operand.type == o_reglist:
        # .specval contains bitmask
        for reg in range(NR_GPREGS):
            if operand.specval & (1 << reg):
                result.add(reg)

        print_debug("o_reglist %s" % result)

    elif operand.type == o_idpspec0:
        # shifted operand
        # .specflag1 is register number for shift-by-register
        # .specflag2 is shift type (0=LSL, 1=LSR, 2=ASR, 4=ROR (amount==0 --> RRX))
        # .value is shift amount
        shift_reg = operand.phrase
        shift_type = operand.specflag2

        # default: shift by number
        shift_by_reg = False
        shift_amount = operand.value

        # other case: shift by register
        if shift_amount == 0:
            shift_by_reg = True
            shift_amount = operand.specflag1
            result.add(shift_amount)

        result.add(shift_reg)

        if shift_by_reg:
            print_debug("o_idpspec0 r%d, %d r%d" % (shift_reg, shift_type, shift_amount))
        else:
            print_debug("o_idpspec0 r%d, %d %d" % (shift_reg, shift_type, shift_amount))

    elif operand.type in [o_void, o_imm, o_near, o_idpspec4, o_idpspec5, o_mem, o_idpspec2, o_idpspec3]:
        # If operand is any of No Operand, Immediate Value, Immediate Near Address (CODE), Seems to be related to
        # vector instructions, Seems to be related to vector instructions,  Direct Memory Reference (DATA),
        # Coprocessor register list, Coprocessor register, then skip processing.
        # Refer to https://github.com/EiNSTeiN-/idapython/blob/master/python/idc.py
        pass

    else:
        assert False, "unknown operand type %d" % operand.type

    return result


def InsRegisters(I):
    referenced_registers = set()
    all_operand_registers = list()

    for operand in I.ops:
        regs = operand_registers(operand)
        referenced_registers |= regs
        all_operand_registers.append(regs)

    return referenced_registers, all_operand_registers


# instructions
def InsIsHanging(i):
    return get_func(i) is None


def InsIsData(i):
    return is_data(get_full_flags(i))


def InsIsUnknown(i):
    return is_unknown(get_full_flags(i))


def InsAssembled(i):
    s = get_bytes(i, get_item_size(i))
    return ''.join(format(c, '02x') for c in s)
    # CHANGED return Dword(i)


def InsIsThumb(i):
    return get_sreg(i, "T") == 1


def InsIsLastInBbl(i, B):
    if (B.start_ea == B.end_ea) and (i == B.start_ea):
        # this is a single-instruction BBL
        return True

    # cross-check instruction address with last address in block
    return i == prev_head(B.end_ea)


def InsIsCall(I):
    cf = I.get_canon_feature()

    return cf & CF_CALL


def CallTargetName(i):
    refs = CodeRefsFrom(i, 0)
    return get_func_name(refs.next())


COND_BRANCHES = [
    NN_ja,
    NN_jae,
    NN_jb,
    NN_jbe,
    NN_jc,
    NN_jcxz,
    NN_je,
    NN_jecxz,
    NN_jg,
    NN_jge,
    NN_jl,
    NN_jle,
    NN_jna,
    NN_jnae,
    NN_jnb,
    NN_jnbe,
    NN_jnc,
    NN_jne,
    NN_jng,
    NN_jnge,
    NN_jnl,
    NN_jnle,
    NN_jno,
    NN_jnp,
    NN_jns,
    NN_jnz,
    NN_jo,
    NN_jp,
    NN_jpe,
    NN_jpo,
    NN_jrcxz,
    NN_js,
    NN_jz
]

UCOND_BRANCHES = [
    NN_jmp,
    NN_jmpfi,
    NN_jmpni,
    NN_jmpshort
]


def InsIsConditional(I):
    """
    opcode = GetMnem(I.ea)
    return I.segpref != CONDITION_AL or opcode == 'CBZ' or opcode == 'CBNZ'
    """
    return I.itype in COND_BRANCHES


def InsIsBranch(I):
    """
    cf = I.get_canon_feature()
    opcode = I.get_canon_mnem()

    return cf & CF_JUMP or \
            opcode == 'B' or \
            opcode == 'CBZ' or \
            opcode == 'CBNZ'
    """
    return InsIsConditional(I) or I.itype in UCOND_BRANCHES


def InsDefinesPC(I):
    # For example, IDA does _not_ mark LDM sp, {pc} as a jump...
    cf = I.get_canon_feature()
    opcode = I.get_canon_mnem()
    referenced_registers, all_operand_registers = InsRegisters(I)

    if opcode == 'LDM':
        return REG_PC in all_operand_registers[1]
    else:
        for op_index, op_regs in enumerate(all_operand_registers):
            if (REG_PC in op_regs) and (cf & (CF_CHG1 << op_index)):
                return True

    return False


def InsIsFlow(I):
    is_call = InsIsCall(I)
    is_branch = InsIsBranch(I)
    is_other = InsDefinesPC(I)
    return is_call or is_branch or is_other, is_call, is_branch or is_other


def InsNotInTextSection(i):
    # seg_name = get_segm_name(getseg(i))
    seg_name = get_segm_name(i)
    return seg_name != ".text", seg_name


def InsString(i):
    return GetDisasm(i)


def InsOpcode(i):
    return print_insn_mnem(i)


def InsNext(I):
    return I.ea + I.size


def InsIsNop(I):
    # SDK: allins.hpp, ARM_nop==2
    return I.itype == 2


def InsIsMacro(I):
    return I.is_macro()


def InsSwitchInfo(i):
    switch_info = get_switch_info(i)
    if switch_info is None:
        return False, None, None

    cases = calc_switch_cases(i, switch_info)
    return bool(cases), switch_info, cases


def SwitchDefault(switch_info):
    # if switch_info.flags & SWI_DEF_IN_TBL:
    if switch_info.flags & SWI_RESERVED:
        return switch_info.defjump

    return None


def SwitchNrCases(switch_info):
    return switch_info.ncases


# basic block
def BblIncomingInsns(i):
    return list(CodeRefsTo(i, 1))


def BblOutgoingInsns(i):
    return list(CodeRefsFrom(i, 1))


# functions
def FunctionCreateAt(i):
    create_insn(i)
    add_func(i)
    auto_wait()


def FunctionAppendChunk(function_address, A, B_ex):
    # CAVEAT this function also adds successor hanging instructions!
    append_func_tail(function_address, A, B_ex)
    auto_wait()


def FunctionRemoveChunk(any_ins_in_chunk):
    chunk_function = get_func_attr(any_ins_in_chunk, FUNCATTR_START)
    remove_fchunk(chunk_function, any_ins_in_chunk)
    auto_wait()


def ChunkMove(any_ins_in_chunk, function_address):
    chunk_start = get_fchunk_attr(any_ins_in_chunk, FUNCATTR_START)
    chunk_end = get_fchunk_attr(any_ins_in_chunk, FUNCATTR_END)

    FunctionRemoveChunk(any_ins_in_chunk)
    FunctionAppendChunk(function_address, chunk_start, chunk_end)


def Dechunkize(inner_start, inner_end_ex):
    outer_function = get_func_attr(inner_start, FUNCATTR_START)
    outer_start = get_fchunk_attr(inner_start, FUNCATTR_START)
    outer_end_ex = get_fchunk_attr(inner_start, FUNCATTR_END)

    # remove chunk from function
    FunctionRemoveChunk(outer_start)

    # add PRE-part to original function, if needed
    if outer_start < inner_start:
        FunctionAppendChunk(outer_function, outer_start, inner_start)

    # add POST-part to original function, if needed
    if inner_end_ex < outer_end_ex:
        FunctionAppendChunk(outer_function, inner_end_ex, outer_end_ex)
