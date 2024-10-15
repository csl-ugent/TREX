import shutil
import os
import time

from core.workspace import Workspace
from modules.base_module import ConvenienceModule
from containers.ida.idarunner import IDARunner
from plugins.csvIO import CsvIO


class IDADism(ConvenienceModule):

    def __init__(self):
        super().__init__()

        self.out_dir = os.path.join(self.tmp_path, 'ida')
        self.disassembler = IDARunner(Workspace.core.docker_client, self.binary_name, self.binary_is64bit, self.out_dir)

    def run(self):
        print("\n--- IDADism ---")
        if not os.path.exists(self.out_dir):
            os.mkdir(self.out_dir)
        # IDARunner mounts a dir into the IDA container were it expects the target binary and will generate output files
        tmp_binary_path = os.path.join(self.out_dir, self.binary_name)
        if not os.path.exists(tmp_binary_path):
            shutil.copyfile(self.binary_path, tmp_binary_path)

        start = time.perf_counter()
        self.disassembler.run()
        end = time.perf_counter()
        print(f'Running IDA disassembler took {end-start} second(s).')

        self.__loadCSV()
        print("IDADism done")

    def __loadCSV(self):
        csv_path = f'{self.binary_name}.ida'
        files = {
            'functions': f'{csv_path}.functions',
            'blocks': f'{csv_path}.bbls',
            'instructions': f'{csv_path}.instructions',
            'edges': f'{csv_path}.edges',
            'calls': f'{csv_path}.calls',
            'strings': f'{csv_path}.strings',
            'stringxrefs': f'{csv_path}.stringxrefs'
        }
        start = time.perf_counter()
        CsvIO.import_disassembly_CSV(self.binary_name, files, self.out_dir)
        end = time.perf_counter()
        print(f'IDA import took {end-start} second(s).')
