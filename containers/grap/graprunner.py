import os

from docker import DockerClient

from containers.containerRunner import ContainerRunner


class GRAPRunner(ContainerRunner):
    """
    Run grap (pattern matcher) within a Docker container.
    """
    def __init__(self, docker_client: DockerClient, cmd: str, pattern_path: str, cfg_path: str, target_path: str):
        super().__init__(
            docker_client,
            image_name="mate_attack_framework_grap",
            command=cmd.format(
                PATTERN=os.path.join("/patterns", os.path.basename(pattern_path)),
                BINARY=os.path.basename(target_path),
                CFG=os.path.join("tmp", os.path.basename(cfg_path))
            ),
            container_build_folder=os.path.join(os.path.dirname(os.path.abspath(__file__)), "container"),
            volumes=ContainerRunner.convert_to_docker_api_volume_structure({
                os.path.dirname(target_path): "/target",
                os.path.dirname(pattern_path): "/patterns"
            })
        )

    def is_ready(self) -> bool:
        return True
