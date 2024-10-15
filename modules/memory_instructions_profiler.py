from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class MemoryInstructionsProfilerModule(ConvenienceModule):
    PREFIX: str = "memoryinstructionsprofiler"

    def __init__(self, binary_params: str, timeout: int, properties_prefix: str = '',
                 recorder: Optional[SDERecorder] = None, instruction_values_limit: Optional[int] = 5):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'memory-instructions-profiler'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX} -instruction_values_limit {instruction_values_limit}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

        # This can be used to run this Pin plugin with different input sizes, for example
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Memory instructions profiler ---")
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
                        LOAD CSV WITH HEADERS FROM 'file:///{MemoryInstructionsProfilerModule.PREFIX}.memory-instructions.csv' AS row
                        MERGE (ins:Instruction {{image_name: row.image_name, image_offset: toInteger(row.image_offset)}})
                        SET ins.ip                                                        = toInteger(row.ip),
                            ins.section_name                                              = row.section_name,
                            ins.routine_name                                              = row.routine_name,
                            ins.routine_offset                                            = toInteger(row.routine_offset),
                            ins.{self.properties_prefix}read_values                       = row.read_values,
                            ins.{self.properties_prefix}written_values                    = row.written_values,
                            ins.{self.properties_prefix}read_values_entropy               = toFloat(row.read_values_entropy),
                            ins.{self.properties_prefix}written_values_entropy            = toFloat(row.written_values_entropy),
                            ins.{self.properties_prefix}num_bytes_read                    = toInteger(row.num_bytes_read),
                            ins.{self.properties_prefix}num_bytes_written                 = toInteger(row.num_bytes_written),
                            ins.{self.properties_prefix}num_unique_byte_addresses_read    = toInteger(row.num_unique_byte_addresses_read),
                            ins.{self.properties_prefix}num_unique_byte_addresses_written = toInteger(row.num_unique_byte_addresses_written),
                            ins.{self.properties_prefix}num_executions                    = toInteger(row.num_executions)
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.memory-instructions.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
