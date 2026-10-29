#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Run after firmware flash, app upload and reset to the Watch face.
python3 -u "${ROOT_DIR}/scripts/watch_ui_serial.py" "$@" smoke
