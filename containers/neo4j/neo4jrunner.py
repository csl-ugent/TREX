import json
import os
from typing import List
from urllib.request import Request, urlopen

from docker import DockerClient

from containers.containerRunner import ContainerRunner
from neo4j_py2neo_bridge import Neo4JCredentials, neo4j_default_credentials


class Neo4JRunner(ContainerRunner):
    @staticmethod
    def check_plugin_presence(plugins_dir: str, required_plugins: List[str]) -> List[str]:
        plugins_dir_count = len([n for n in os.listdir(plugins_dir) if os.path.isfile(os.path.join(plugins_dir, n))])

        if plugins_dir_count == 0:
            print(f"WARNING - Neo4J plugin folder empty - Will try to download them at container startup!")
            return required_plugins

        if len(required_plugins) != plugins_dir_count:
            for plugin in required_plugins:
                plugin_path = os.path.join(plugins_dir, f'{plugin}.jar')
                if not os.path.exists(plugin_path):
                    print(f"WARNING - Neo4J is missing plugin '{plugin}'")

        return []

    def __init__(self, docker_client: DockerClient, data_dir: str, import_dir: str, plugins_dir: str,
                 credentials: Neo4JCredentials = neo4j_default_credentials, db_port: int = 7687, web_port: int = 7474):
        super().__init__(
            docker_client,
            image_name="neo4j:5.4-enterprise",
            container_name="mate_framework_neo4j",
            environment={
                "NEO4J_AUTH": "/".join(credentials),
                "NEO4J_dbms_directories_import": "",
                "NEO4J_dbms.security.allow__csv__import__from__file__urls": "true",
                "NEO4J_apoc_export_file_enabled": "true",
                "NEO4J_ACCEPT_LICENSE_AGREEMENT": "yes",
                "NEO4J_PLUGINS": json.dumps(self.check_plugin_presence(plugins_dir, ["apoc", "graph-data-science"])),
                "NEO4J_dbms_security_procedures_unrestricted": "*",
                "NEO4J_dbms_security_auth__minimum__password__length": 4,
                "NEO4J_server_memory_heap_max__size": "10G",
                "NEO4J_server_jvm_additional": "-Xss128M",
            },
            ports={
                "7474/tcp": str(web_port),
                "7687/tcp": str(db_port)
            },
            volumes={
                data_dir: {'bind': '/data', 'mode': 'rw'},
                import_dir: {'bind': '/var/lib/neo4j/import', 'mode': 'rw'},
                plugins_dir: {'bind': '/plugins', 'mode': 'rw'}
            },
            detach=True,
            startup_timeout=20
        )
        self._db_port = db_port
        self._web_port = web_port

    def is_ready(self) -> bool:
        if self.is_running():
            request = Request(f"http://localhost:{self._web_port}")
            request.get_method = lambda: 'HEAD'

            try:
                urlopen(request)
                return True
            except Exception:
                pass

        return False
