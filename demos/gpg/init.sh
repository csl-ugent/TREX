#!/usr/bin/env bash
set -Eeuo pipefail

# Commands that need to be run before GPG.

# Starting GPG agent as a daemon.
chmod +x ./gpg-agent
LD_LIBRARY_PATH=. ./gpg-agent --daemon
