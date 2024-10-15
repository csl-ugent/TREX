#!/usr/bin/env bash

cd "$( dirname "${BASH_SOURCE[0]}" )"

pushd container

docker build .
IMAGE_ID="$(docker build -q .)"

TMPDIR="$(mktemp -d -t --suffix=.SUFFIX test_sh.XXXXXX)"
trap "rm -rf '$TMPDIR'" 0               # EXIT
trap "rm -rf '$TMPDIR'; exit 1" 2       # INT
trap "rm -rf '$TMPDIR'; exit 1" 1 15    # HUP TERM

popd

for tool in sources/*/; do
    [[ "$tool" == "sources/cmake/" ]] && continue

    echo "===================================================================="
    echo "Testing Pin tool: $tool"
    echo "===================================================================="

    docker run -i --rm -v "$TMPDIR:/target" -v "$PWD:/host" -w /host --entrypoint bash "$IMAGE_ID" -s <<EOF

    pushd "$tool" &>/dev/null

    mkdir -p /tmp/build
    cmake -GNinja -B/tmp/build
    ninja -C/tmp/build check && echo "OK: All tests passed." || echo "ERR: Some tests failed."

    popd &>/dev/null
EOF

    read
done
