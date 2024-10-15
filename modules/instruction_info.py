from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class InstructionInfoModule(ConvenienceModule):
    PREFIX: str = "instructioninfo"

    def __init__(self, binary_params: str, timeout: int, recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'instruction-info'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

    def run(self):
        print("\n--- Instruction info ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index
            db.create_index("Instruction", ["image_name", "image_offset"])

            # Import instructions
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{InstructionInfoModule.PREFIX}.instruction-info.csv' AS row
                        MERGE (ins:Instruction {{image_name: row.image_name, image_offset: toInteger(row.image_offset)}})
                        SET ins.full_image_name                        = row.full_image_name,
                            ins.section_name                           = row.section_name,
                            ins.routine_name                           = row.routine_name,
                            ins.routine_offset                         = toInteger(row.routine_offset),
                            ins.opcode                                 = row.opcode,
                            ins.operands                               = row.operands,
                            ins.category                               = row.category,
                            ins.DEBUG_filename                         = row.DEBUG_filename,
                            ins.DEBUG_line                             = toInteger(row.DEBUG_line),
                            ins.DEBUG_column                           = toInteger(row.DEBUG_column),
                            ins.bytes                                  = row.bytes
                        }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.instruction-info.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
