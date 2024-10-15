import yara

from core.core import Core
from core.workspace import Workspace
from graph_models.grap_analysis import PatternOccurrence


class Yara:
    def __init__(self, target=None, rules_path=None):
        self.binary_path = Workspace.current.binary_path
        self.target = target
        self.rules_path = rules_path or Core().get_subdirectory('modules', 'yara', 'crypto.yar')
        
    def run(self):
        print("\n--- Yara ---")
        
        print("Compiling rules..")
        rules = yara.compile(self.rules_path)
        
        print("Matching..")
        matches = rules.match(self.target or self.binary_path)
        return matches
        # print("Mapping results..")
        # analysis = self.map_results(matches)
        # return analysis
    
    def map_results(self, matches):
        analysis_node = "kek"
        for pattern in matches:
            # pattern_node.name = pattern.rule
            for match in pattern.strings:
                pattern_node = PatternOccurrence()
                pattern_node.pattern = pattern.rule
                pattern_node.start_address = match[0]
                pattern_node.end_address = match[0] + len(match[2])
