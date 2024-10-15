from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class BranchProfilerModule(ConvenienceModule):
    PREFIX: str = "branchprofiler"

    def __init__(self, binary_params: str, timeout: int, properties_prefix: str = '',
                 recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'branch-profiler'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

        # This can be used to run this Pin plugin with different input sizes, for example
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Branch profiler ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index on Instruction for performance reasons
            db.create_index("Instruction", ["image_name", "image_offset"])

            # Import basic blocks
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{BranchProfilerModule.PREFIX}.branches.csv' AS row
                        MERGE (ins:Instruction {{image_name: row.image_name, image_offset: toInteger(row.image_offset)}})
                        SET ins.{self.properties_prefix}num_branch_taken     = toInteger(row.num_taken),
                            ins.{self.properties_prefix}num_branch_not_taken = toInteger(row.num_not_taken)
                        }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.branches.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
