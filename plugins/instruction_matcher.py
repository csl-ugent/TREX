from typing import List, Callable

from core.workspace import Workspace
from graph_models.disassembly_analysis import Instruction


def match_assembly(source: Instruction, target: Instruction) -> bool:
    return source.assembly == target.assembly


def never_match(source: Instruction, target: Instruction) -> bool:
    return False


InstructionMatchFilterType = Callable[[Instruction, Instruction], bool]


class InstructionMatch:
    def __init__(self, source: Instruction):
        self.source = source
        self._matches: List[Instruction] = []
        self._specific_matches: List[Instruction] = []

    def append_match(self, match: Instruction, add_to_specific_matches: bool):
        if add_to_specific_matches:
            self._specific_matches.append(match)
        else:
            self._matches.append(match)

    def get_matches(self, get_specific_matches: bool = False) -> List[Instruction]:
        return self._specific_matches if get_specific_matches else self._matches


class InstructionMatcher:
    def __init__(self, source, target, match_filter: InstructionMatchFilterType = never_match):
        self.match_filter = match_filter
        self.source_analysis = source['analysis']
        self.source_rel = self._verify_path(source['path'], 's')
        self.target_analysis = target['analysis']
        self.target_rel = self._verify_path(target['path'], 't')

        query = "MATCH (sA:Analysis), (tA:Analysis) WHERE id(sA) = $source_id AND id(tA) = $target_id WITH sA, tA\n"
        query += "MATCH {} WITH tA, s\n".format(self.source_rel)
        query += "OPTIONAL MATCH {}\n".format(self.target_rel)
        query += """
                 WHERE (s.image_name = t.image_name) AND (s.image_offset = t.image_offset)
                 RETURN s AS source_ins, COLLECT(t) AS target_ins ORDER BY source_ins.address
                 """
        self.query = query

    def _verify_path(self, path: str, source: str):
        ins_node = "({}:Instruction)"
        analysis_node = "({}:Analysis)"

        ins_node_pattern = ins_node.format('target_instruction')
        analysis_node_pattern = analysis_node.format('source_analysis')
        ins_node_pos = path.find(ins_node_pattern)
        analysis_node_pos = path.find(analysis_node_pattern)

        if ins_node_pos == -1 or analysis_node_pos == -1:
            raise ValueError("Path needs to specify both a {} and a {} node".format(ins_node_pattern,
                                                                                    analysis_node_pattern))

        return path.replace(ins_node_pattern, ins_node.format(source)).replace(analysis_node_pattern,
                                                                               analysis_node.format(source+'A'))

    def run(self) -> List[InstructionMatch]:
        return self.match_address()

    def match_address(self) -> List[InstructionMatch]:
        matches = []
        cursor = Workspace.current.graph.run(self.query, {
            'source_id': self.source_analysis.__primaryvalue__,
            'target_id': self.target_analysis.__primaryvalue__
        })

        while cursor.forward():
            source_ins = Instruction.wrap(cursor.current['source_ins'])
            match = InstructionMatch(source_ins)

            for node in cursor.current['target_ins']:
                target_ins = Instruction.wrap(node)
                match.append_match(target_ins, self.match_filter(source_ins, target_ins))

            matches.append(match)

        return matches
