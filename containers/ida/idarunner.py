import os

from docker import DockerClient

from core.core import Core
from containers.containerRunner import ContainerRunner


class IDARunner(ContainerRunner):
    """
    Runs IDA (pro) in a Docker container with a plugin to extract data in an automated way.
    """
    def __init__(self, docker_client: DockerClient, binary_name: str, binary_64_bit: bool, out_dir: str):
        super().__init__(
            docker_client,
            image_name="mate_attack_framework_ida",
            container_build_folder=os.path.join(os.path.dirname(os.path.abspath(__file__)), "container"),
            container_build_options={
                "IDA_PASSWORD": IDARunner.get_installation_password()
            },
            volumes=ContainerRunner.convert_to_docker_api_volume_structure({
                out_dir: '/target',
                Core().get_subdirectory("containers", "ida", "plugins"): '/plugins'
            }),
            privileged=True,
            use_display=True,
            command=['ida_plugin.py', binary_name, str(binary_64_bit)],
            run_as_native_user=False
        )

    def is_ready(self) -> bool:
        return True

    @classmethod
    def get_installation_password(cls) -> str:
        """
        Retrieve the installation password for the IDA pro installation.
        :return: The installation password
        """
        folder = os.path.dirname(os.path.abspath(__file__))
        file = os.path.join(folder, "container", "installer", "ida.installpwd")
        with open(file, "r") as fh:
            file_contents = fh.readline()

        return file_contents
