import inspect
import os
import time
from typing import AnyStr

import docker
from docker import DockerClient
from neo4j_py2neo_bridge import GraphService, neo4j_default_credentials

from containers.neo4j.neo4jrunner import Neo4JRunner


class Core:
    _instance: 'Core' = None

    def __new__(cls, db_port: int = 7687, web_port: int = 7474):
        """
        Return a singleton instance of the Core class.
        :param db_port: The database port to connect to.
        """
        if cls._instance is None:
            cls._instance = super(Core, cls).__new__(cls)

            cls._instance._root_dir = cls._get_root_dir()
            cls._instance._docker_client = docker.from_env()

            # Start database container if not running already
            credentials = neo4j_default_credentials
            db_runner = Neo4JRunner(
                cls._instance.docker_client,
                cls._instance.get_subdirectory('data'),
                cls._instance.neo4j_import_directory,
                cls._instance.get_subdirectory('neo4j_plugins'),
                credentials,
                db_port,
                web_port
            )
            db_runner.startup()
            # Connect to database
            # Retry a couple of times, in case the database has not initialised completely
            max_num_retries = 9
            delay = 0.1
            for i in range(max_num_retries):
                try:
                    cls._instance.db = GraphService(f"bolt://localhost:{db_port}", auth=credentials)
                except:
                    if i == max_num_retries - 1:
                        raise
                    else:
                        print(f'Could not connect to database. Sleeping for {delay} seconds before trying again...')
                        time.sleep(delay)
                        delay *= 2.0
                else:
                    break

        return cls._instance

    @classmethod
    def _get_root_dir(cls):
        filename = inspect.getframeinfo(inspect.currentframe()).filename
        path = os.path.dirname(os.path.abspath(filename))

        root_path = os.path.join(path, '..')
        return os.path.abspath(root_path)

    def get_subdirectory(self, *paths: AnyStr) -> AnyStr:
        return os.path.join(self._root_dir, *paths)

    @property
    def neo4j_import_directory(self) -> AnyStr:
        return self.get_subdirectory("import")

    @property
    def docker_client(self) -> DockerClient:
        return self._docker_client
