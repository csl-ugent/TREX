from graph_models.analysis import Analysis
from graph_models.annotations import annotatable, annotator

# BROKEN


class Disassembly(Analysis):
    __primarylabel__ = 'Disassembly'

    basic_blocks = RelatedTo('BasicBlock', 'DEFINES_BLOCK')
    functions = RelatedTo('Function', 'DEFINES_FUNCTION')

    def __init__(self):
        super(Disassembly, self).__init__()


class Function(Model):
    name = Property()

    basic_blocks = RelatedTo('BasicBlock', 'HAS_BLOCK')
    analysis = RelatedFrom('Disassembly', 'DEFINES_FUNCTION')


@annotatable
@annotator
class BasicBlock(Model):
    image_name = Property()
    image_offset_begin = Property()
    image_offset_end = Property()

    instructions = RelatedTo('Instruction', 'HAS_INSTRUCTION')
    analysis = RelatedFrom('Disassembly', 'DEFINES_BLOCK')
    functions = RelatedFrom('Function', 'HAS_BLOCK')
    out_set = RelatedTo('BasicBlock', 'HAS_CF_EDGE')
    in_set = RelatedFrom('BasicBlock', 'HAS_CF_EDGE')


@annotatable
@annotator
class Instruction(Model):
    image_name = Property()
    image_offset = Property()
    assembly = Property()
    mnem = Property()
    operands = Property()

    basic_block = RelatedFrom('BasicBlock', 'HAS_INSTRUCTION')
