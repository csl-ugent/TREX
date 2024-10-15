import os
import re
import shutil
from subprocess import PIPE, run
from typing import Dict, AnyStr, List, Callable, Union, Optional

from neo4j.exceptions import ResultNotSingleError

from neo4j_py2neo_bridge import Graph

from containers.pin.sderunner import SDERecorder, get_tool_architecture
from core.core import Core


def determine_64bit(binary_path: str) -> bool:
    completed = run(['file', binary_path], stdout=PIPE)
    bits = re.findall(r"(?:.*): ELF (\d+)-bit .*", str(completed.stdout))

    if len(bits) == 0:
        raise ValueError("Failed to extract ELF architecture")

    return int(bits[0]) == 64


class Workspace:
    workspaces: Dict[str, 'Workspace'] = {}
    current: 'Workspace' = None
    core: Core = None

    @staticmethod
    def is_workspace_graph(graph: Graph) -> bool:
        try:
            graph.run_evaluate('MATCH (n:workspace_info) RETURN n.binary_name')
            return True
        except ResultNotSingleError:
            return False

    @staticmethod
    def load_from_db(core: Core):
        print("Available workspaces: ")
        Workspace.core = core
        for graph_name in core.db:
            if graph_name not in ['system', 'neo4j'] and Workspace.is_workspace_graph(core.db[graph_name]):
                print(graph_name)
                Workspace.workspaces[graph_name] = Workspace(core.db[graph_name])

    @staticmethod
    def create_new(name: str, path_to_binary: str, clear_db_default: Optional[bool]=None, clear_workspace_default: Optional[bool]=None):
        if name[:1].isdigit():
            raise ValueError("Workspace names cannot start with a digit!")

        if name in Workspace.workspaces:
            print(f"Workspace already exists: `{name}`")
            clear_db = clear_db_default if clear_db_default is not None else (input("Drop database? (y/n) ") == 'y')
            if clear_db:
                print(f'Dropping database `{name}`')
                Workspace.core.db.system_graph.run(f"drop database `{name}`")

        # Create new database
        Workspace.core.db.system_graph.run(f"create database `{name}` if not exists")
        workspace_graph = Workspace.core.db[name]

        # Create new workspace file structure
        workspace_path = Workspace.core.get_subdirectory("workspaces", name)
        if os.path.exists(workspace_path):
            print(f"Workspace folder already exists: {workspace_path}")
            clear_workspace = clear_workspace_default if clear_workspace_default is not None else (input("Clear folder? (y/n) ") == 'y')
            if clear_workspace:
                print(f'Clearing workspace `{name}`')
                shutil.rmtree(workspace_path)

        os.makedirs(workspace_path, exist_ok=True)

        # Copy binary
        binary_name = os.path.basename(path_to_binary)
        binary_path = os.path.join(workspace_path, binary_name)
        shutil.copyfile(path_to_binary, binary_path)
        binary_is64bit = determine_64bit(binary_path)

        # Insert workspace info node
        workspace_graph.run(f'''
                            MERGE (wsi:workspace_info {{binary_name: "{binary_name}", binary_is64_bit: {binary_is64bit}}})
                            ''')

        # Register in application
        Workspace.workspaces[name] = Workspace(workspace_graph)

    @staticmethod
    def select(name: str):
        Workspace.current = Workspace.workspaces[name]

        # Create tmp dir
        if not os.path.exists(Workspace.current.tmp_path):
            os.mkdir(Workspace.current.tmp_path)
        os.chdir(Workspace.current.tmp_path)
        print(f"Workspace.current = `{name}`\n")

    def __init__(self, workspace_graph: Graph):
        self.graph = workspace_graph
        self.name = workspace_graph.name
        self.binary_name = workspace_graph.run_evaluate('MATCH (wsi:workspace_info) RETURN wsi.binary_name')
        self.binary_is64bit = workspace_graph.run_evaluate('MATCH (wsi:workspace_info) RETURN wsi.binary_is64_bit')
        self.path = Workspace.core.get_subdirectory("workspaces", self.name)
        self.binary_path = os.path.join(self.path, self.binary_name)
        self.tmp_path = os.path.join(self.path, 'tmp')

    def import_csv_files(self, source_folder: AnyStr, files: Union[List[AnyStr], Dict[AnyStr, AnyStr]],
                         execute_queries: Callable[[Graph], None]) -> None:
        """
        Import CSV files into the database.

        @param source_folder: The source folder where the csv's to be imported are located.
        @param files: The file names to be imported. Either a list or a dictionary where the destination name is the
        key and the source name is the value.
        @param execute_queries: The callback function that will be called to execute the import queries.
        """

        if isinstance(files, list):
            files = {f: f for f in files}

        for target, source in files.items():
            src = os.path.join(source_folder, source)
            dest = os.path.join(Workspace.core.neo4j_import_directory, target)
            shutil.copyfile(src, dest)
            run(['chmod', 'a+r', dest])

        execute_queries(self.graph)

        for file in files:
            file_to_delete = os.path.join(Workspace.core.neo4j_import_directory, file)
            os.remove(file_to_delete)

    def create_recorder(self,
                        binary_params: Optional[str] = None,
                        pinball_suffix: Optional[str] = None,
                        force_record: bool = False,
                        libc_tunables: Optional[str] = None,
                        interposer: Optional[str] = None,
                        extra_command: Optional[str] = None) -> SDERecorder:
        return SDERecorder(Core().docker_client, self.binary_path, binary_params, pinball_suffix, force_record,
                           get_tool_architecture(self.binary_is64bit), libc_tunables=libc_tunables,
                           interposer=interposer, extra_command=extra_command)
