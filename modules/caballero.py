from typing import Optional
import time

from core.core import Core
from modules.base_module import ConvenienceModule
from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.workspace import Workspace
from neo4j_py2neo_bridge import Graph


class Caballero(ConvenienceModule):
    PREFIX: str = "caballero"

    def __init__(self, ratio: float, binary_params: str, timeout: int,
            properties_prefix: str = '', recorder: Optional[SDERecorder] = None):
        """Initialize a Caballero module
        param ratio: integer: minimum ratio of crypto related instructions
        param binary_params: string: commandline params to pass onto the target binary
        param(optional) timeout: integer: execution will halt after 'timeout' seconds
        """
        super().__init__()

        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'caballero'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX} -r {ratio}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Caballero ---")
        self.runner.run()

        print("\nImporting pattern matches..")
        results = self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index
            db.create_index('BasicBlock', ['image_name', 'image_offset_begin', 'image_offset_end'])

            # Import basic blocks
            db.run(f'''
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{Caballero.PREFIX}.caballero.csv' AS row
                        MERGE (bbl:BasicBlock
                            {{image_name: row.image_name,
                            image_offset_begin: toInteger(row.image_offset_begin),
                            image_offset_end: toInteger(row.image_offset_end)}})
                        SET bbl.{self.properties_prefix}caballero = true
                    }} IN TRANSACTIONS OF 1000 ROWS
                    ''')

        Workspace.current.import_csv_files(
                self.workspace_path,
                [f'{self.PREFIX}.caballero.csv'],
                execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
