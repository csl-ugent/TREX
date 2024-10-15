from subprocess import run, PIPE
import os
from typing import Tuple

from core.workspace import Workspace


class MemoryMap:
    def __init__(self):
        self.binary_path = Workspace.current.binary_path
        self.tmp_path = Workspace.current.tmp_path
        
    def run(self):
        print("\n--- MemoryMap ---")
        text_label = 't'
        text_query = self._map_section('.text', text_label)
        rodata_label = 'r'
        rodata_query = self._map_section('.rodata', rodata_label)

        # Assemble query to create Analysis and sections
        analysis_label = 'a'
        query = f"""
            CREATE ({analysis_label}:MemoryMap {{module: 'MemoryMap', source: 'binary'}}),
                    {text_query},
                    {rodata_query},
                    ({analysis_label})-[h_{text_label}:HAS_SECTION]->({text_label}),
                    ({analysis_label})-[h_{rodata_label}:HAS_SECTION]->({rodata_label})
        """
        Workspace.current.graph.run(query)

    def _map_section(self, section_name: str, query_label: str) -> str:
        print(f"\tMapping section '{section_name}'..")
        offset, size = self._get_section_bounds(section_name)
        body = self._get_section_body(section_name)

        # Create resulting query
        query = f"""
            ({query_label}:Section {{ source: '{section_name}', offset: '{offset}', size: '{size}' }}),
            ({query_label}_sb:SectionBody {{ body: '{body}' }}),
            ({query_label})-[{query_label}_hb:HAS_BODY]->({query_label}_sb)
        """
        return query
        
    def _get_section_bounds(self, section_name: str) -> Tuple[int, int]:
        cmd = ["readelf", "-S", self.binary_path]
        print(f"\t\tRunning readelf ({' '.join(cmd)}) ...", end=" ")
        completed = run(cmd, stdout=PIPE)
        output = str(completed.stdout)
        start = output.find(section_name)
        output = output[start:start+100].replace("\\n", "").split()
        offset = int(output[3], 16)
        size = int(output[4], 16)
        print("done")
        
        return offset, size
    
    def _get_section_body(self, section_name: str) -> str:
        output_file = os.path.join(self.tmp_path, f'{section_name}.raw')
        cmd = ["objcopy", "-O", "binary", f"--only-section={section_name}", self.binary_path, output_file]
        print(f"\t\tRunning objcopy ({' '.join(cmd)}) ...", end=" ")
        run(cmd)
        print("done")
        
        body = ""
        with open(output_file, 'rb') as f:
            body = ''.join('{:02x}'.format(x) for x in f.read())
        
        return body
