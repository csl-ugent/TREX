from abc import ABC, abstractmethod
import os
import shutil
from typing import Optional

from core.core import Core
from core.workspace import Workspace

################
#  Base class  #
################

class Application(ABC):
    '''
    Abstract base class for functionality specific to the benchmarked application.
    '''

    def create_input_file(self, name: str, size_in_bytes: int) -> None:
        '''
        Creates an input file of a given size (in bytes) with random contents.

        Parameters:
        - name (str): The name of the file.
        - size_in_bytes (int): The filesize in bytes.
        '''
        # NOTE: We need to write random data to ensure that the compressed size
        # scales linearly with this size of the input file.
        with open(name, 'wb') as f:
            f.write(os.urandom(size_in_bytes))

    @abstractmethod
    def get_output_filename(self) -> str:
        '''
        Get the name of the output file.

        Returns:
        - str: The name of the output file.
        '''
        pass

    def remove_output_file(self) -> None:
        '''
        Removes the output file of the application (if it exists).
        You need to call this function before you run the application,
        as otherwise the application will just exit because the output
        file already exists.
        '''
        archive_path = os.path.join(self.workspace.path, self.get_output_filename())

        if os.path.isfile(archive_path):
            os.remove(archive_path)

    @abstractmethod
    def get_executable_path(self) -> str:
        '''
        Get the path of the main executable.

        Returns:
        - str: The path to the main executable.
        '''
        pass

    def get_executable_path_lldb(self) -> str:
        '''
        Get the path of the main executable for LLDB.

        Returns:
        - str: The path to the main executable.
        '''
        return self.get_executable_path()

    @abstractmethod
    def get_crypto_image_path(self) -> str:
        '''
        Get the path of the image containing the cryptographic function(s).

        Returns:
        - str: The path to the crypto executable or library.
        '''
        pass

    @abstractmethod
    def get_binary_params(self, input_file: str, perform_encryption: bool = True, generic_deobf: bool = False, passphrase: Optional[str] = 'secret') -> str:
        '''
        Get the binary parameters that need to be passed to the benchmark application.

        Parameters:
        - input_file (str): The name of the input file.
        - perform_encryption (bool): True if the application needs to perform encryption,
          False otherwise.
        - generic_deobf (bool): True if we need to run under the generic deobfuscator,
          False otherwise.
        - passphrase (str): The passphrase to use.

        Returns:
        - str: The binary parameters to use.
        '''
        pass

    @abstractmethod
    def create_workspace(self, clear_db: Optional[bool], clear_workspace: Optional[bool]) -> None:
        '''
        Creates the workspace for the application.

        Parameters:
        - clear_db (Optional[bool]): An optional boolean indicating whether or
          not to drop the database.
        - clear_workspace (Optional[bool]): An optional boolean indicating
          whether or not to drop the workspace.
        '''
        pass

    @abstractmethod
    def get_step_1_input_sizes(self) -> list:
        '''
        Get the set of input sizes (in bytes) that is used for step 1 of the localisation process.
        '''
        pass

    @abstractmethod
    def get_generic_deobf_libc_tunables(self) -> str:
        '''
        Get the libc tunables to use for recording the pinballs for this
        application, in order to get it working with the generic deobfuscator.
        '''
        return None

    @abstractmethod
    def get_generic_deobf_interposer_path(self) -> str:
        '''
        Get the interposer path to be used for recording the pinballs for this
        application, in order to get it working with the generic deobfuscator.
        '''
        return None

    @abstractmethod
    def get_generic_deobf_calls_csv_path(self) -> str:
        '''
        Get the CSV path to be used as input for the generic deobfuscator
        taint analysis.
        '''
        return None

    @abstractmethod
    def get_generic_deobf_input_file_path(self) -> str:
        '''
        Get the file path to be used as input for the application,
        for the generic deobfuscator pinball.
        '''
        return None

    @abstractmethod
    def get_datadeps_path_length_threshold(self) -> float:
        '''
        Get the threshold to use for data deps distance.
        '''
        return None

###########
#  7-Zip  #
###########

class SzipApplication(Application):
    def get_output_filename(self) -> str:
        return 'test.7z'

    def get_executable_path(self) -> str:
        return Core().get_subdirectory('demos', 'sevenzip-x64', '7za')

    def get_executable_path_lldb(self) -> str:
        return Core().get_subdirectory('demos', 'sevenzip-x64', '7za')

    def get_crypto_image_path(self) -> str:
        return Core().get_subdirectory('demos', 'sevenzip-x64', '7za')

    def get_binary_params(self, input_file: str, perform_encryption: bool = True, generic_deobf: bool = False, passphrase: Optional[str] = 'secret') -> str:
        if perform_encryption:
            return f'-mmt1 -p{passphrase} a test.7z {input_file}'
        else:
            return f'-mmt1 a test.7z {input_file}'

    def create_workspace(self, clear_db: Optional[bool], clear_workspace: Optional[bool]) -> None:
        executable_path = self.get_executable_path()

        Workspace.create_new('szipx64', executable_path, clear_db_default=clear_db, clear_workspace_default=clear_workspace)
        Workspace.select('szipx64')

        self.workspace = Workspace.current
        self.db = Workspace.current.graph

    def get_step_1_input_sizes(self) -> list:
        sizes_kb = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000]
        return [x * 1000 for x in sizes_kb]

    def get_generic_deobf_libc_tunables(self) -> str:
        return 'glibc.cpu.hwcaps=-AVX_Usable,-AVX2_Usable,-AVX_Fast_Unaligned_Load'

    def get_generic_deobf_interposer_path(self) -> str:
        return None

    def get_generic_deobf_calls_csv_path(self) -> str:
        return Core().get_subdirectory('demos', 'sevenzip-x64', '7za-calls.csv')

    def get_generic_deobf_input_file_path(self) -> str:
        return Core().get_subdirectory('demos', 'sevenzip-x64', 'gen_deobf_input')

    def get_datadeps_path_length_threshold(self) -> float:
        return 1000

#########
#  GPG  #
#########

class GpgApplication(Application):
    def get_output_filename(self) -> str:
        return 'encrypted'

    def get_executable_path(self) -> str:
        return Core().get_subdirectory('demos', 'gpg', 'gpg')

    def get_crypto_image_path(self) -> str:
        return Core().get_subdirectory('demos', 'gpg', 'libgcrypt.so.20')

    def get_binary_params(self, input_file: str, perform_encryption: bool = True, generic_deobf: bool = False, passphrase: Optional[str] = 'secret') -> str:
        if perform_encryption:
            return f'--homedir /tmp --output encrypted --symmetric --cipher-algo AES256 --passphrase {passphrase} --s2k-count 1025 --batch {input_file}'
        else:
            # To disable encryption for GPG, we just use a different cipher.
            return f'--homedir /tmp --output encrypted --symmetric --cipher-algo 3DES --passphrase secret --s2k-count 1025 --batch {input_file}'

    def create_workspace(self, clear_db: Optional[bool], clear_workspace: Optional[bool]) -> None:
        executable_path = self.get_executable_path()

        Workspace.create_new('gpg', executable_path, clear_db_default=clear_db, clear_workspace_default=clear_workspace)
        Workspace.select('gpg')

        self.workspace = Workspace.current
        self.db = Workspace.current.graph

        # Copy over the needed shared libraries.
        executable_dir = os.path.dirname(executable_path)
        for file in os.listdir(executable_dir):
            if '.so' in file or file == 'gpg-agent' or file == 'init.sh':
                src = os.path.join(executable_dir, file)
                dst = os.path.join(self.workspace.path, file)
                shutil.copyfile(src, dst)

    def get_step_1_input_sizes(self) -> list:
        sizes_kb = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000]
        return [x * 1000 for x in sizes_kb]

    def get_generic_deobf_libc_tunables(self) -> str:
        return 'glibc.cpu.hwcaps=-AVX_Usable,-AVX2_Usable,-AVX_Fast_Unaligned_Load'

    def get_generic_deobf_interposer_path(self) -> str:
        return Core().get_subdirectory('demos', 'gpg', 'interposer.c')

    def get_generic_deobf_calls_csv_path(self) -> str:
        return Core().get_subdirectory('demos', 'gpg', 'gpg_calls.csv')

    def get_generic_deobf_input_file_path(self) -> str:
        return Core().get_subdirectory('demos', 'gpg', 'gen_deobf_input')

    def get_datadeps_path_length_threshold(self) -> float:
        return 10
