from idaapi import BADADDR, ea_t, void

ARGV = ...

o_void = ...
o_reg = ...
o_mem = ...
o_phrase = ...
o_displ = ...
o_imm = ...
o_far = ...
o_near = ...
o_idpspec0 = ...
o_idpspec1 = ...
o_idpspec2 = ...
o_idpspec3 = ...
o_idpspec4 = ...
o_idpspec5 = ...
o_trreg = ...
o_dbreg = ...
o_crreg = ...
o_fpreg = ...
o_mmxreg = ...
o_xmmreg = ...
o_reglist = ...
o_creglist = ...
o_creg = ...
o_fpreglist = ...
o_text = ...
o_cond = ...
o_spr = ...
o_twofpr = ...
o_shmbme = ...
o_crf = ...
o_crb = ...
o_dcr = ...

FUNCATTR_START   =  ...
FUNCATTR_END     =  ...
FUNCATTR_FLAGS   =  ...
FUNCATTR_FRAME   = ...
FUNCATTR_FRSIZE  = ...
FUNCATTR_FRREGS  = ...
FUNCATTR_ARGSIZE = ...
FUNCATTR_FPD     = ...
FUNCATTR_COLOR   = ...
FUNCATTR_OWNER   = ...
FUNCATTR_REFQTY  = ...

AU_UNK = ...
AU_CODE = ...

def get_root_filename(*args): ...
def get_bytes(ea, size, use_dbg = False): ...
def get_item_size(ea): ...
def get_sreg(ea, reg): ...
def prev_head(ea, minea=0): ...
def next_head(ea, maxea=BADADDR): ...
def get_func_name(ea): ...
def get_segm_name(ea): ...
def GetDisasm(ea): ...
def print_insn_mnem(ea): ...
def print_operand(ea, n): ...
def create_insn(*args) -> "int": ...
def add_func(*args) -> "bool": ...
def auto_wait(*args) -> "bool": ...
def append_func_tail(funcea, ea1, ea2): ...
def get_func_attr(ea, attr): ...
def get_fchunk_attr(ea, attr): ...
def remove_fchunk(funcea, tailea): ...
def qexit(*args) -> "void": ...
def get_item_head(*args) -> "ea_t": ...
def auto_mark_range(*args) -> "void": ...
def patch_byte(*args) -> "bool": ...
def create_byte(*args) -> "bool": ...
def plan_and_wait(sEA, eEA, final_pass=True): ...