from typing import Optional
import time

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class MemoryBuffersModule(ConvenienceModule):
    PREFIX: str = "memorybuffer"

    def __init__(self, binary_params: str, timeout: int, recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'memory-buffer'
        self.pin_tool_params = f"-csv_prefix {self.PREFIX}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

    def run(self):
        print("\n--- Memory buffers ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # --------------
            # Create indices
            # --------------

            db.create_index("Instruction", ["ip"])
            db.create_index("Instruction", ["image_name", "image_offset"])

            for node in ['MemoryBuffer', 'MemoryRegion']:
                db.create_index(node, ["id"])

            # -----------------
            # Load instructions
            # -----------------

            db.run(f"""
                   CALL {{
                    LOAD CSV WITH HEADERS FROM 'file:///{MemoryBuffersModule.PREFIX}.instructions.csv' AS row
                    MERGE (ins:Instruction {{image_name: row.image_name, image_offset: toInteger(row.image_offset)}})
                    SET ins.ip                     = row.ip,
                        ins.section_name           = row.section_name,
                        ins.routine_name           = row.routine_name,
                        ins.mnem                   = row.opcode,
                        ins.operands               = row.operands,
                        ins.read_values            = row.read_values,
                        ins.written_values         = row.written_values,
                        ins.read_values_entropy    = toFloat(row.read_values_entropy),
                        ins.written_values_entropy = toFloat(row.written_values_entropy),
                        ins.num_bytes_read         = toInteger(row.num_bytes_read),
                        ins.num_bytes_written      = toInteger(row.num_bytes_written)
                    }} IN TRANSACTIONS OF 1000 ROWS
                   """)

            # ------------------------
            # Load buffers and regions
            # ------------------------

            for (file, node) in [('buffers', 'MemoryBuffer'),
                                 ('regions', 'MemoryRegion')]:
                spatial_metrics = ['bit_shannon', 'byte_shannon', 'byte_shannon_adapted', 'byte_num_different', 'byte_num_unique', 'bit_average', 'byte_average']
                temporal_metrics = ['bit_shannon']

                entropy_fields = [f'average_spatial_entropy_{metric}' for metric in spatial_metrics] + [f'average_temporal_entropy_{metric}' for metric in temporal_metrics]

                entropy_query = ', '.join([f'buf.{field} = toFloat(row.{field})' for field in entropy_fields])

                db.run(f"""
                        CALL {{
                            LOAD CSV WITH HEADERS FROM 'file:///{MemoryBuffersModule.PREFIX}.{file}.csv' AS row
                            CREATE (buf:{node} {{id: row.id}})
                            SET buf.start_address              = toInteger(row.start_address),
                                buf.end_address                = toInteger(row.end_address),
                                buf.num_reads                  = toInteger(row.num_reads),
                                buf.num_writes                 = toInteger(row.num_writes),
                                buf.allocation_address         = toInteger(row.allocation_address),
                                buf.DEBUG_allocation_backtrace = row.DEBUG_allocation_backtrace,
                                buf.DEBUG_annotation           = row.DEBUG_annotation,
                                {entropy_query}
                        }} IN TRANSACTIONS OF 1000 ROWS
                    """)

            # -------------------------------------------
            # Load links buffers/regions and instructions
            # -------------------------------------------

            for (file, node) in [('buffers', 'MemoryBuffer'),
                                 ('regions', 'MemoryRegion')]:
                for (property, relation) in [('read_ips', 'READS_FROM'),
                                             ('write_ips', 'WRITES_TO')]:
                    db.run(f"""
                            CALL {{
                                LOAD CSV WITH HEADERS FROM 'file:///{MemoryBuffersModule.PREFIX}.{file}.csv' AS row
                                MERGE (buf:{node} {{id: row.id}})
                                WITH buf, row
                                UNWIND
                                    CASE
                                        WHEN size(row.{property}) = 0
                                        THEN []
                                        ELSE split(row.{property}, ',')
                                    END AS ip
                                MERGE (ins:Instruction {{ip: ip}})
                                MERGE (ins) -[:{relation}]-> (buf)
                            }} IN TRANSACTIONS OF 1000 ROWS
                            """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.instructions.csv', f'{self.PREFIX}.buffers.csv', f'{self.PREFIX}.regions.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
