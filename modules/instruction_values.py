from typing import Optional
import os
import time
import tempfile

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule


class InstructionValuesModule(ConvenienceModule):
    PREFIX: str = "instructionvalues"

    def __init__(self, binary_params: str, timeout: int, instruction_values_limit: int = -1,
                 instruction_ranges: list = [], properties_prefix: str = '', recorder: Optional[SDERecorder] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'instruction-values'
        self.instruction_ranges_file = tempfile.NamedTemporaryFile(mode='w', dir=Workspace.current.path)

        for (image_name, begin_offset, end_offset) in instruction_ranges:
            print(f'{image_name} {begin_offset} {end_offset}', file=self.instruction_ranges_file)
        self.instruction_ranges_file.flush()

        ranges_file_arg = os.path.basename(self.instruction_ranges_file.name)
        self.pin_tool_params = f"-csv_prefix {self.PREFIX} -instruction_values_limit {instruction_values_limit} -ranges_file {ranges_file_arg}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)

        # This can be used to run this Pin plugin with different input sizes, for example
        self.properties_prefix = properties_prefix

    def run(self):
        print("\n--- Instruction values ---")
        self.runner.run()

        print("\nImporting matches..")
        self.__import_results()
        print("\nMatches imported")

    def __import_results(self):
        import_start = time.perf_counter()

        def execute_queries(db: Graph) -> None:
            # Create index
            db.create_index("Instruction", ["image_name", "image_offset"])

            # create query for operands
            # NOTE: Keep this in sync with the value in containers/pin/sources/instruction-values/src/main.cpp
            MAX_NUM_OPERANDS = 4
            operand_query = ','.join([f'''
            ins.{self.properties_prefix}operand_{op}_repr           = CASE WHEN {op} < toInteger(row.num_operands) THEN row.operand_{op}_repr ELSE null END,
            ins.{self.properties_prefix}operand_{op}_width          = CASE WHEN {op} < toInteger(row.num_operands) THEN toInteger(row.operand_{op}_width) ELSE null END,
            ins.{self.properties_prefix}operand_{op}_is_read        = CASE WHEN {op} < toInteger(row.num_operands) THEN toInteger(row.operand_{op}_is_read) <> 0 ELSE null END,
            ins.{self.properties_prefix}operand_{op}_is_written     = CASE WHEN {op} < toInteger(row.num_operands) THEN toInteger(row.operand_{op}_is_written) <> 0 ELSE null END,
            ins.{self.properties_prefix}operand_{op}_read_values    = CASE WHEN {op} < toInteger(row.num_operands) THEN row.operand_{op}_read_values ELSE null END,
            ins.{self.properties_prefix}operand_{op}_written_values = CASE WHEN {op} < toInteger(row.num_operands) THEN row.operand_{op}_written_value ELSE null END
                                       ''' for op in range(0, MAX_NUM_OPERANDS)])

            # Import instructions
            db.run(f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///{InstructionValuesModule.PREFIX}.instruction-values.csv' AS row
                        MERGE (ins:Instruction {{image_name: row.image_name, image_offset: toInteger(row.image_offset)}})
                        SET ins.{self.properties_prefix}num_operands   = toInteger(row.num_operands),
                            {operand_query}
                        }} IN TRANSACTIONS OF 1000 ROWS
                    """)

        Workspace.current.import_csv_files(
            self.workspace_path,
            [f'{self.PREFIX}.instruction-values.csv'],
            execute_queries)

        import_end = time.perf_counter()
        print(f'Importing results took {import_end-import_start} second(s).')
