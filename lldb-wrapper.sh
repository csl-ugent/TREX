#!/usr/bin/env bash
set -Eeuo pipefail

usage()
{
    cat <<EOF >&2
Usage: $0 LLDB_EXECUTABLE [OPTION]...

Wrapper around LLDB to expose the framework's Python API inside LLDB's
interactive Python interpreter.

Runs LLDB using the command LLDB_EXECUTABLE and the given OPTIONs.

Options:
-h, --help          Show this help.
EOF
}

positional=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage; exit 0
            ;;
        -*)
            echo "Unknown command-line option '$1'."
            echo "Try '$0 --help' for more information."
            exit 1
            ;;
        *)
            positional+=("$1")
            shift
            ;;
    esac
done
set -- "${positional[@]}"

if [[ $# -eq 0 ]]; then
    echo "Expected at least one positional argument, but got $#."
    echo "Try '$0 --help' for more information."
    exit 1
fi

LLDB_EXECUTABLE="${1:-lldb}"
shift

export PYTHONPATH="$(python3 -c 'import sys; print(":".join(sys.path))')"

"$LLDB_EXECUTABLE" "$@"
