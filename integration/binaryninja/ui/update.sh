#!/usr/bin/env bash
set -Eeuo pipefail

cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

pyside6-uic InstructionInfoDock.ui >ui_instructioninfodock.py
pyside6-uic MemoryBufferInfoDock.ui >ui_memorybufferinfodock.py
pyside6-uic MBADetectDock.ui >ui_mbadetectdock.py