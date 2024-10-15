import os

from docker import DockerClient

from containers.containerRunner import ContainerRunner

class LLDBRunner(ContainerRunner):
    """
    Run LLDB (Low Level Debugger, part of the LLVM project) in a container.
    """

    def __init__(self, docker_client: DockerClient):
        super().__init__(
                docker_client,
                image_name="mate_framework_lldb",
                container_name="mate_framework_lldb",
                container_build_folder=os.path.join(os.path.dirname(os.path.abspath(__file__)), "container"),
                privileged=True,
                detach=True,
                ports={
                    f'{port}/tcp': f'{port}'
                    for port in [1234, *range(31200, 31300 + 1)]
                }
        )
