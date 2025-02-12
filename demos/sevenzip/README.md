# p7zip

The source code of p7zip is available at https://sourceforge.net/projects/p7zip/. The currently used version is 16.02.

A pre-built executable is checked in into the repo, but you can rebuild p7zip using the `build.sh` script in this directory. Note that this build is Dockerized, so you need to have `docker` installed.

## Patched version

To patch the 7-Zip binary in order to reduce the number of KDF iterations, follow the following process:
1. Open the `7za` binary in BinaryNinja
2. Go to the instruction `0xb30c6 mov dword [esi+0x1c], 0x13`
3. Patch the instruction to `mov dword [esi+0x1c], 0x0` (i.e., change the constant to `0`) using the option `Patch > Edit current line` in the BinaryNinja context menu.
4. Do the same for the following instructions:
    - `0xb627f mov edi, 100`: patch to `mov edi, 1`
    - `0xb622c mov dword [esp+0xc], 1000`: patch to `mov dword [esp+0xc], 1`
5. Save the file as `7za-double-patched`.
