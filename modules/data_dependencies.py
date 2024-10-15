from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class DataDependenciesModule(ConvenienceModule):
    PREFIX: str = "data_dependencies"

    def __init__(self, binary_params: str, timeout: int,
                 relationship_tag: str = '',
                 shortcuts: bool = False,
                 ignore_rsp: bool = False,
                 ignore_rip: bool = False,
                 ignore_rbp_stack_memory: bool = False,
                 syscall_file: Optional[str] = None,
                 recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.relationship_tag = relationship_tag
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'data-dependencies'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}" + \
                               (" -shortcuts" if shortcuts else "") + \
                               (f" -syscall_file {syscall_file}"  if syscall_file is not None else "") + \
                               (" -ignore_rsp" if ignore_rsp else "") + \
                               (" -ignore_rip" if ignore_rip else "") + \
                               (" -ignore_rbp_stack_memory" if ignore_rbp_stack_memory else "")
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

    def run(self):
        print("\n--- Data dependencies ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index on Instruction for performance reasons
            db.create_index("Instruction", ["image_name", "image_offset"])

            for (csv_file, relationship_property, csv_property) in [('memory_dependencies', 'address', 'Memory'), ('register_dependencies', 'register', 'Register')]:
                # NOTE: We split the import of nodes and relations to improve
                # performance.

                # Import from nodes.
                db.run(f"""
                        CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{DataDependenciesModule.PREFIX}.{csv_file}.csv' AS line
                        MERGE (from_ins: Instruction {{image_name: line.Write_img, image_offset: toInteger(line.Write_off)}})
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

                # Import to nodes.
                db.run(f"""
                        CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{DataDependenciesModule.PREFIX}.{csv_file}.csv' AS line
                        MERGE (to_ins: Instruction {{image_name: line.Read_img, image_offset: toInteger(line.Read_off)}})
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

                # Import DEPENDS_ON relation.
                db.run(f"""
                        CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{DataDependenciesModule.PREFIX}.{csv_file}.csv' AS line
                        MATCH (from_ins: Instruction {{image_name: line.Write_img, image_offset: toInteger(line.Write_off)}})
                        MATCH (to_ins: Instruction {{image_name: line.Read_img, image_offset: toInteger(line.Read_off)}})
                        CREATE (from_ins)<-[:DEPENDS_ON {{{relationship_property}: line.{csv_property}, tag: "{self.relationship_tag}"}}]-(to_ins)
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{DataDependenciesModule.PREFIX}.syscall_instructions.csv' AS line
                        MERGE (syscall:SystemCall {{thread_id: line.thread_id, syscall_id: line.syscall_id}})
                        SET syscall.syscall_file_index = toInteger(line.index)
                        MERGE (ins:Instruction {{image_name: line.image_name, image_offset: toInteger(line.image_offset)}})
                        CREATE (syscall) -[:INVOKED_BY]-> (ins)
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.memory_dependencies.csv', f'{self.PREFIX}.register_dependencies.csv', f'{self.PREFIX}.syscall_instructions.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
