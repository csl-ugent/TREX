from typing import Callable, List

from core.workspace import Workspace
from graph_models.analysis import Analysis
from graph_models.disassembly_analysis import BasicBlock

BasicBlockMatchFilterType = Callable[[BasicBlock, BasicBlock], bool]


def match_bytes(source_block: BasicBlock, target_block: BasicBlock) -> bool:
    for source_instruction in source_block.instructions:
        target_instruction = target_block.instructions.match().where(image_name=source_instruction.image_name, image_offset=source_instruction.image_offset).first()
        if target_instruction and source_instruction.assembly != target_instruction.assembly:
            return False
    return True


def never_match(source_block: BasicBlock, target_block: BasicBlock) -> bool:
    return False


class BasicBlockMatch:
    def __init__(self, source: BasicBlock):
        self.source = source
        self._matched_blocks = []
        self._matched_type_blocks = []

    def append_match(self, target_block: BasicBlock, add_to_match_type: bool):
        if add_to_match_type:
            self._matched_type_blocks.append(target_block)
        else:
            self._matched_blocks.append(target_block)

    def get_matches(self, get_specific_match_type_blocks: bool = False) -> List[BasicBlock]:
        return self._matched_type_blocks if get_specific_match_type_blocks else self._matched_blocks


class BasicBlockMatcher:
    def __init__(self, source: Analysis, target: Analysis, match_filter: BasicBlockMatchFilterType = never_match):
        self.match_filter = match_filter
        self.source_analysis = source
        self.target_analysis = target

    def run(self) -> List[BasicBlockMatch]:
        return self.match_address()

    def match_address(self) -> List[BasicBlockMatch]:
        source_blocks = []

        query = """
                MATCH (sA:Analysis), (tA:Analysis) WHERE id(sA) = $source_id AND id(tA) = $target_id
                MATCH (s)<-[:RECORDED_BLOCK]-(sA) WITH tA, s
                OPTIONAL MATCH (t)<-[:DEFINES_BLOCK]-(tA)
                WHERE (s.image_name = t.image_name) AND (s.image_offset_begin < t.image_offset_end) AND (s.image_offset_end > t.image_offset_begin)
                RETURN s AS source_block, COLLECT(t) AS target_blocks ORDER BY source_block.image_offset_begin
                """

        cursor = Workspace.current.graph.run(query, {
            'source_id': self.source_analysis.__primaryvalue__,
            'target_id': self.target_analysis.__primaryvalue__
        })

        while cursor.forward():
            source_block = BasicBlock.wrap(cursor.current['source_block'])
            match = BasicBlockMatch(source_block)

            for node in cursor.current['target_blocks']:
                target_block = BasicBlock.wrap(node)
                match.append_match(target_block, self.match_filter(source_block, target_block))

            source_blocks.append(match)

        return source_blocks


class BasicBlockByteMatcher(BasicBlockMatcher):
    def __init__(self, source, target):
        super().__init__(source, target, match_bytes)
