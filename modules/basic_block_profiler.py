from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class BasicBlockProfilerModule(ConvenienceModule):
    PREFIX: str = "basicblockprofiler"

    def __init__(self, binary_params: str, timeout: int, properties_prefix: str = '',
                 recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'basic-block-profiler'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

        # This can be used to run this Pin plugin with different input sizes, for example
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Basic block profiler ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index
            db.create_index('BasicBlock', ['image_name', 'image_offset_begin', 'image_offset_end'])

            # Import basic blocks
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{BasicBlockProfilerModule.PREFIX}.basic-blocks.csv' AS row
                        MERGE (bbl:BasicBlock {{image_name: row.image_name, image_offset_begin: toInteger(row.image_offset_begin), image_offset_end: toInteger(row.image_offset_end)}})
                        SET bbl.ip_begin                               = toInteger(row.ip_begin),
                            bbl.ip_end                                 = toInteger(row.ip_end),
                            bbl.section_name                           = row.section_name,
                            bbl.routine_name                           = row.routine_name,
                            bbl.routine_offset_begin                   = toInteger(row.routine_offset_begin),
                            bbl.routine_offset_end                     = toInteger(row.routine_offset_end),
                            bbl.{self.properties_prefix}num_executions = toInteger(row.num_executions)
                        }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.basic-blocks.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
