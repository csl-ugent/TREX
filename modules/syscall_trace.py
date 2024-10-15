from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class SysCallTraceModule(ConvenienceModule):
    PREFIX: str = "syscall-trace"

    def __init__(self, binary_params: str, timeout: int,
                 properties_prefix: str = '',
                 recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'syscall-trace'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

        # This can be used to run this Pin plugin with different input sizes, for example
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Syscall Trace ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # NOTE: Keep this in sync with the value in containers/pin/sources/syscall-trace/src/main.cpp
            MAX_SYSCALL_ARGUMENTS = 6

            arguments_query = ','.join([f'''
                       syscall.{self.properties_prefix}argument_{i}  = CASE WHEN {i} < toInteger(row.num_arguments) THEN row.argument_{i} ELSE null END
            ''' for i in range(0, MAX_SYSCALL_ARGUMENTS)])

            db.run(f"""
                   CALL {{
                    LOAD CSV WITH HEADERS FROM 'file:///{SysCallTraceModule.PREFIX}.system-calls.csv' AS row
                    MERGE (syscall:SystemCall {{thread_id: row.thread_id, syscall_id: row.syscall_id}})
                    SET syscall.{self.properties_prefix}name          = row.name,
                        syscall.{self.properties_prefix}num_arguments = row.num_arguments,
                        { arguments_query },
                        syscall.{self.properties_prefix}return_value  = row.return_value
                   }} IN TRANSACTIONS OF 1000 ROWS
                   """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.system-calls.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
