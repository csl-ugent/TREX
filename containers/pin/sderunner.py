import os
from enum import Enum
from typing import Optional, Union, AnyStr

from docker import DockerClient
from docker.models.containers import Container

from containers.containerRunner import ContainerRunner


class PINArchitecture(Enum):
    IA32 = "ia32"
    IA64 = "intel64"


def get_tool_architecture(is64bit: bool) -> PINArchitecture:
    return PINArchitecture.IA64 if is64bit else PINArchitecture.IA32


def get_uid_gid_string() -> str:
    # Support for Windows, which has no os.getuid()
    try:
        return f"{os.getuid()}:{os.getgid()}"
    except AttributeError:
        return ""


class SDERunner(ContainerRunner):
    def __init__(self,
                 docker_client: DockerClient,
                 binary_path: str,
                 binary_params: str,
                 pinball_suffix: Optional[str] = None,
                 tool_architecture: PINArchitecture = PINArchitecture.IA32,
                 timeout: Optional[int] = 0,
                 **docker_run_parameters):
        super().__init__(
            docker_client,
            image_name="mate_attack_framework_sde",
            container_build_folder=os.path.join(os.path.dirname(os.path.abspath(__file__)), "container"),
            volumes=ContainerRunner.convert_to_docker_api_volume_structure({
                os.path.dirname(os.path.abspath(binary_path)): '/target',
                SDERunner.compiled_tool_path(): "/compiled_tools",
                SDERunner.source_tool_path(): "/source_tools"
            }),
            run_as_native_user=False,
            privileged=True,
            **docker_run_parameters
        )
        self.tool_architecture = tool_architecture
        self.binary_path = binary_path
        self.binary_params = binary_params
        self.timeout = timeout
        self.pinball_suffix = pinball_suffix

    def is_ready(self) -> bool:
        return True

    @classmethod
    def compiled_tool_path(cls) -> str:
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), "plugins")

    @classmethod
    def source_tool_path(cls) -> str:
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), "sources")

    @property
    def path_to_sde(self):
        return "/sde/sde" if self.tool_architecture == PINArchitecture.IA32 else "/sde/sde64"

    @property
    def target_path(self):
        return f"/target/{os.path.basename(self.binary_path)}"

    @property
    def pinball_path(self):
        suffix = ('_' + self.pinball_suffix) if self.pinball_suffix is not None else ''
        return f"/target/{os.path.basename(self.binary_path)}.pinball{suffix}"


class SDERecorder(SDERunner):
    """
    Record a pinball using SDE (Software Development Emulator) in a container.
    """

    def __init__(self,
                 docker_client: DockerClient,
                 binary_path: str,
                 binary_params: str,
                 pinball_suffix: Optional[str] = None,
                 force_record: bool = False,
                 tool_architecture: PINArchitecture = PINArchitecture.IA32,
                 timeout: Optional[int] = 0,
                 libc_tunables: Optional[str] = None,
                 interposer: Optional[str] = None,
                 extra_command: Optional[str] = None,
                 stdin_input:Optional[str]=None):
        
        stdin_open = detach = (stdin_input is not None)
        
        super().__init__(docker_client, binary_path, binary_params, pinball_suffix, tool_architecture, timeout, stdin_open=stdin_open, detach=detach)
        self.force_record = force_record
        self.has_recorded = False
        if not self.force_record:
            self.has_recorded = os.path.exists(
                os.path.join(os.path.dirname(binary_path), os.path.basename(self.pinball_path) + '.text'))
        self.libc_tunables = libc_tunables
        self.interposer = interposer
        self.extra_command = extra_command
        self.stdin_input = stdin_input if stdin_input is None or stdin_input.endswith("\n") else f"{stdin_input}\n"

    def run(self) -> Union[Container, AnyStr]:
        if not self.force_record and self.has_recorded:
            print("Skip recording (already exists)")
            return ""
        print("Recording pinball...")
        # %sde -log -log:basename %t/pinball -- %t.exe
        sde_command = f"{self.path_to_sde} " \
                      f"-log -log:basename {self.pinball_path} -log:mt " \
                      f"-- {self.target_path} {self.binary_params}"

        self._docker_command = [
            sde_command,
            str(self.timeout),
            os.path.basename(self.binary_path),
            get_uid_gid_string(),
            self.libc_tunables if self.libc_tunables is not None else "",
            self.interposer if self.interposer is not None else "",
            self.extra_command if self.extra_command is not None else ""
        ]
        result = super().run()

        if self.stdin_input is not None:
            sock = self._docker_client.api.attach_socket(result.id, params={"stdin": True, "stream": True})
            if os.name == "nt":
                # Windows
                sock.send(self.stdin_input.encode("utf-8"))
                result.wait()
                sock.close()
            else:
                # Linux (not tested on other *NIX systems)
                sock._sock.send(self.stdin_input.encode("utf-8"))
                result.wait()
                sock._sock.close()
                sock.close()
        
        self.has_recorded = True
        return result


class SDEReplayer(SDERunner):
    """
    Run SDE (Software Development Emulator) in a container with a given plugin for Pin (SDE is built on top of Intel
    Pin).
    """

    def __init__(self, docker_client: DockerClient, binary_path: str, binary_params: str, tool_name: str,
                 tool_params: str, tool_architecture: PINArchitecture = PINArchitecture.IA32,
                 timeout: Optional[int] = 0, recorder: Optional[SDERecorder] = None,
                 stdin_input: Optional[str] = None):
        super().__init__(docker_client, binary_path, binary_params,
                         None if recorder is None else recorder.pinball_suffix, tool_architecture, timeout)
        self.tool_name = tool_name
        self.tool_params = tool_params
        self.built_plugin = False
        self.full_pin_tool_name = self.build_plugin(self.tool_architecture, self.tool_name)
        self.nullapp_path = f"/sde/{self.tool_architecture.value}/nullapp"
        self.recorder = recorder
        if self.recorder is None:
            self.recorder = SDERecorder(docker_client, binary_path, binary_params, self.pinball_suffix, False,
                                        tool_architecture, timeout, stdin_input=stdin_input)

    def is_ready(self) -> bool:
        return True

    def run(self) -> Union[Container, AnyStr]:
        self.build_plugin(self.tool_architecture, self.tool_name)
        if not self.recorder.has_recorded:
            self.recorder.run()
        print(f"Running replay for {self.tool_name}")
        # %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
        sde_command = f"{self.path_to_sde} " \
                      f"-t ../../compiled_tools/{self.full_pin_tool_name} {self.tool_params} " \
                      f"-replay -replay:basename {self.pinball_path} -replay:addr_trans " \
                      f"-- {self.nullapp_path}"

        self._docker_command = [
            sde_command,
            str(self.timeout),
            os.path.basename(self.binary_path),
            get_uid_gid_string()
        ]
        return super().run()

    def build_plugin(self, architecture: PINArchitecture, tool_name: str) -> str:
        """
        Builds the plugin for PIN and returns the full path for in the container.

        :param architecture: The architecture to build the plugin with.
        :param tool_name: The name of the pin tool (the name of the folder under sources for this plugin).
        :return: The file path inside the docker container.
        """
        plugin_compiled_path = f"{architecture.value}_{tool_name}.so"
        if self.built_plugin:
            return plugin_compiled_path

        print(f"Building plugin for PIN: {tool_name} on {architecture.value}")
        self.build_container()
        self._docker_client.containers.run(
            self._docker_image_name,
            entrypoint="/build_plugin.sh",
            command=[architecture.value, tool_name],
            name=self._docker_container_name,
            remove=True,
            volumes=self._docker_volumes,
            **self._docker_kwargs
        )
        self.built_plugin = True
        return plugin_compiled_path
