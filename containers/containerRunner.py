import os
import random
import string
from abc import abstractmethod
from time import sleep
from typing import Optional, Dict, Union, List, AnyStr

from docker import DockerClient
from docker.errors import ImageNotFound, ContainerError
from docker.models.containers import Container

SimpleVolumes = Dict[str, str]
DockerVolumes = Dict[str, Dict[str, str]]


localRandom = random.Random()


class ContainerRunner:
    def __init__(self, docker_client: DockerClient, image_name: str, container_name: Optional[str] = None,
                 container_build_folder: Optional[str] = None, container_build_options: Optional[Dict[str, str]] = None,
                 command: Optional[Union[str, List[str]]] = None, environment: Optional[Dict[str, str]] = None,
                 ports: Optional[Dict[str, str]] = None, volumes: Optional[DockerVolumes] = None,
                 detach: bool = False, startup_timeout: int = 0, allow_non_zero_exit_code: bool = False,
                 use_display: bool = False, run_as_native_user: bool = True, **additional_docker_run_parameters):
        """
        Initialize the ContainerRunner instance.

        :param docker_client: The Docker API client instance
        :param image_name: The name of the image to use.
        :param container_name: Optional. The name of the container once it's running.
        :param container_build_folder: Optional. The absolute path to a folder containing a Dockerfile to build this
        image if it's not an image that can be pulled from the docker repository.
        :param container_build_options: Optional. If a Dockerfile is built, pass additional build variables here.
        :param command: Optional. The command to pass to the container
        :param environment: Optional. Environment variables for the container.
        :param ports: Optional. Ports to be mapped to the host.
        :param volumes: Optional. Volumes to mount inside the container.
        :param detach: Detach the container, or use blocking?
        :param startup_timeout: Optional. Timeout in seconds used for letting the container boot if you need the service
        to be available on return of the method.
        :param allow_non_zero_exit_code: Optional. Allow the docker container to exit with a non-zero status.
        :param use_display: Optional. If enabled, additional volumes and environment variables will be passed to the
        Docker container.
        :param run_as_native_user: Optional. If enabled (true by default), run as the user that's running the
        framework rather than root.
        :param additional_docker_run_parameters: Optional. Other keyword parameters that are passed to
        `DockerClient.containers.run`.
        """
        self._docker_client = docker_client
        self._docker_image_name = image_name
        self._docker_command = command
        self._docker_container_name = container_name or \
            f"mate_framework_{''.join(localRandom.choice(string.ascii_lowercase) for _ in range(16))}"
        self._docker_container_build_folder = container_build_folder
        self._docker_container_build_options = container_build_options
        self._docker_env = environment or {}
        self._docker_ports = ports or {}
        self._docker_volumes = volumes or {}
        self._docker_detach = detach
        self._startup_timeout = startup_timeout
        self._docker_allow_non_zero_exit = allow_non_zero_exit_code
        self._docker_kwargs = additional_docker_run_parameters
        if use_display:
            self._docker_env.update({
                "DISPLAY": os.getenv('DISPLAY')
            })
            self._docker_volumes.update(ContainerRunner.get_docker_x_volumes())
        if run_as_native_user:
            self._docker_kwargs['user'] = f'{os.getuid()}:{os.getgid()}'

    def run(self) -> Union[Container, AnyStr]:
        """
        Run the Docker container if it's not already running. If the container is not yet present, it will pull or build
        the container first.
        :return: A container (when detached) or the output of the container when running non-detached.
        """
        print(f"Run {self._docker_image_name} with name {self._docker_container_name}...")
        if not self.is_running():
            self.build_container()
            print(f"\tStarting {self._docker_container_name}...")
            try:
                return self._docker_client.containers.run(
                    self._docker_image_name,
                    command=self._docker_command,
                    name=self._docker_container_name,
                    detach=self._docker_detach,
                    remove=True,
                    environment=self._docker_env,
                    ports=self._docker_ports,
                    volumes=self._docker_volumes,
                    **self._docker_kwargs
                )
            except ContainerError:
                if not self._docker_allow_non_zero_exit:
                    raise

    def build_container(self) -> None:
        """
        Check if the image is present, and attempt to pull or build it if it's not present.
        :return: None.
        """
        if not self._docker_container_build_folder:
            print(f"\tPulling {self._docker_image_name} from repository...", end=' ')
            self._docker_client.images.pull(self._docker_image_name)
            print("done")
        else:
            print(f"\tBuilding {self._docker_image_name} from {self._docker_container_build_folder}...", end=' ')
            self._docker_client.images.build(path=self._docker_container_build_folder,
                                                buildargs=self._docker_container_build_options,
                                                tag=self._docker_image_name)
            print("done")

    def is_running(self) -> bool:
        """
        Check if the container is running by querying the running containers for the container's name.
        :return: True if the container is running, False otherwise.
        """
        return self._docker_client.containers.list(filters={'name': self._docker_container_name})

    @abstractmethod
    def is_ready(self) -> bool:
        """
        Check if the container is ready for use.
        :return: True if the container is ready for use, False otherwise.
        """
        pass

    def startup(self) -> bool:
        """
        Run the container and wait for a maximum defined amount of time for the container to be ready.
        :return: True if the container is ready, False otherwise.
        """
        self.run()
        tries_left = self._startup_timeout
        while not self.is_ready() and tries_left > 0:
            sleep(1)
            tries_left -= 1

        return self.is_ready()

    @classmethod
    def convert_to_docker_api_volume_structure(cls, simple_volumes: SimpleVolumes) -> DockerVolumes:
        """
        Converts simple dictionaries (host path: container path) to Docker API compatible dictionaries. Will always set
        the mode to read/write.
        :param simple_volumes: The simple volumes to convert.
        :return: A Docker API compatible dictionary structure.
        """
        docker_volumes = {}
        for volume, bind in simple_volumes.items():
            docker_volumes[volume] = {'bind': bind, 'mode': 'rw'}
        return docker_volumes

    @classmethod
    def get_docker_x_volumes(cls) -> DockerVolumes:
        """
        Return the necessary volumes to make X pass-through to the container from the host possible.
        :return: A DockerVolumes structure with the necessary volumes defined.
        """
        return {
            "/tmp/.X11-unix": {
                "bind": "/tmp/.X11-unix",
                "mode": "rw"
            },
            os.path.join(os.getenv("HOME"), ".Xauthority"): {
                "bind": "/root/.Xauthority",
                "mode": "rw"
            }
        }
