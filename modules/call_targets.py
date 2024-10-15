from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class CallTargetsModule(ConvenienceModule):
    PREFIX: str = "calltargets"

    def __init__(self, binary_params: str, timeout: int, properties_prefix: str = '',
                 recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'call-targets'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

        # This can be used to run this Pin plugin with different input sizes, for example
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Call targets ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index
            db.create_index("Instruction", ["image_name", "image_offset"])

            # NOTE: We split the import of nodes and relations to improve
            # performance.

            # Import call nodes.
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{CallTargetsModule.PREFIX}.call-targets.csv' AS row
                        MERGE (call: Instruction {{image_name: row.image_name, image_offset: toInteger(row.image_offset)}})
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

            # Import callee nodes.
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{CallTargetsModule.PREFIX}.call-targets.csv' AS row
                        MERGE (callee: Instruction {{image_name: row.target_image_name, image_offset: toInteger(row.target_image_offset)}})
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

            # Import CALLS relation.
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{CallTargetsModule.PREFIX}.call-targets.csv' AS row
                        MATCH (call: Instruction {{image_name: row.image_name, image_offset: row.image_offset}})
                        MATCH (callee: Instruction {{image_name: row.target_image_name, image_offset: toInteger(row.target_image_offset)}})
                        CREATE (call)-[:CALLS]->(callee)
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.call-targets.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
