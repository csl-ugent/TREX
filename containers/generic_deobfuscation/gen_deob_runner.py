import os
from typing import Optional, List

from docker import DockerClient

from containers.containerRunner import ContainerRunner


class GenericDeobfuscationRunner(ContainerRunner):
    """
    Run the Generic Deobfuscation tool in a Docker container.
    """
    def __init__(self, docker_client: DockerClient, absolute_trace_path: str, args: Optional[List[str]] = None):
        deobf_args = []
        if args is not None:
            deobf_args.extend(args)
        deobf_args.append(os.path.basename(absolute_trace_path))
        super().__init__(
            docker_client,
            image_name="mate_attack_framework_gendeob",
            container_build_folder=os.path.join(os.path.dirname(os.path.abspath(__file__)), "container"),
            volumes=ContainerRunner.convert_to_docker_api_volume_structure({
                os.path.dirname(absolute_trace_path): "/target"
            }),
            command=deobf_args
        )

    def is_ready(self) -> bool:
        return True
