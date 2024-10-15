import inspect
import os
import shutil

from core.core import Core
from core.workspace import Workspace
from modules.generic_deobfuscator import GenericDeobfuscator
from modules.syscall_trace import SysCallTraceModule


def demo():
    filename = inspect.getframeinfo(inspect.currentframe()).filename
    path = os.path.dirname(os.path.abspath(filename))
    binary_name = "gpg"
    demofile = os.path.join(path, binary_name)
    Workspace.create_new('gpg', demofile)
    Workspace.select('gpg')
    workspace = Workspace.current

    deobf_csv = "gpg_calls.csv"
    interposer = "interposer.c"
    readme = "gen_deobf_input"

    # Copy all needed files to workspace folder
    files_to_copy = [
        readme, interposer, deobf_csv,
        "gpg-agent", "init.sh", "libassuan.so.0", "libgcrypt.so.20", "libgpg-error.so.0", "libz.so.1"
    ]
    for file_to_copy in files_to_copy:
        file_to_copy_path = Core().get_subdirectory('demos', 'gpg', file_to_copy)
        shutil.copyfile(file_to_copy_path, os.path.join(workspace.path, file_to_copy))

    # Set binary parameters
    binary_params = f"--output encrypted --symmetric --cipher-algo AES256 --passphrase secret --s2k-count 1025 --batch {readme}"
    libc_tunables = "glibc.cpu.hwcaps=-AVX_Usable,-AVX2_Usable,-AVX_Fast_Unaligned_Load"
    extra_command = f"rm -f encrypted && " \
                    f"strace -E LD_PRELOAD=./interposer.so -E GLIBC_TUNABLES={libc_tunables} " \
                    f"-qqq -e t=read -e s=none -y -o {binary_name}.strace {binary_name} {binary_params}"

    recorder = Workspace.current.create_recorder(
        binary_params, libc_tunables=libc_tunables, interposer=interposer, extra_command=extra_command)
    recorder.run()

    # FIXME: automatically parse strace -> csv for generic deobfuscator
    # Hardcoded currently (committed) as gpg_calls.csv

    # Apply SysCall Trace Pin Plugin
    syscall_trace = SysCallTraceModule(binary_params, 0, recorder=recorder)
    syscall_trace.run()

    # Apply Generic deobfuscator
    generic_deobfuscator = GenericDeobfuscator(binary_params, 0, recorder, ["-T", deobf_csv])
    generic_deobfuscator.run()
