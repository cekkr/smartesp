#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT_DIR}/wasm_apps/clock_demo/clock_demo.c"
OUT_DIR="${ROOT_DIR}/wasm_apps/build"
OUT_WASM="${OUT_DIR}/clock_demo.wasm"
OUT_HEADER="${ROOT_DIR}/components/smart_os/src/apps/clock_demo_wasm.h"
EMCC_BIN="${EMCC:-emcc}"
EMCC_ENV=()

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "error: missing command: $1" >&2
        exit 1
    }
}

python_supports_emscripten() {
    local python_bin="$1"
    [ -n "${python_bin}" ] || return 1
    [ -x "${python_bin}" ] || return 1

    "${python_bin}" - <<'PY' >/dev/null 2>&1
import sys
raise SystemExit(0 if sys.version_info >= (3, 10) else 1)
PY
}

detect_homebrew_emscripten_root() {
    local emcc_path="$1"
    local prefix=""

    case "${emcc_path}" in
        /opt/homebrew/*)
            prefix="/opt/homebrew"
            ;;
        /usr/local/*)
            prefix="/usr/local"
            ;;
        *)
            return 1
            ;;
    esac

    ls -1d "${prefix}/Cellar/emscripten/"*/libexec 2>/dev/null | sort | tail -n 1
}

prepare_emcc_env() {
    local emcc_path emscripten_root llvm_root binaryen_root emcc_python candidate

    emcc_path="$(command -v "${EMCC_BIN}")"
    emscripten_root="$(detect_homebrew_emscripten_root "${emcc_path}" || true)"
    llvm_root="${EM_LLVM_ROOT:-}"
    binaryen_root="${EM_BINARYEN_ROOT:-}"
    emcc_python="${EMSDK_PYTHON:-}"

    if ! python_supports_emscripten "${emcc_python}"; then
        emcc_python=""
        for candidate in \
            "$(command -v python3 2>/dev/null || true)" \
            /opt/homebrew/bin/python3 \
            /usr/local/bin/python3; do
            if python_supports_emscripten "${candidate}"; then
                emcc_python="${candidate}"
                break
            fi
        done
    fi
    if [ -n "${emcc_python}" ]; then
        EMCC_ENV+=("EMSDK_PYTHON=${emcc_python}")
    fi

    if [ -n "${emscripten_root}" ]; then
        if [ -z "${llvm_root}" ] && [ -x "${emscripten_root}/llvm/bin/clang" ]; then
            llvm_root="${emscripten_root}/llvm/bin"
        fi
        if [ -z "${binaryen_root}" ] && [ -x "${emscripten_root}/binaryen/bin/wasm-opt" ]; then
            binaryen_root="${emscripten_root}/binaryen"
        fi
    fi
    if [ -n "${llvm_root}" ]; then
        EMCC_ENV+=("EM_LLVM_ROOT=${llvm_root}")
    fi
    if [ -n "${binaryen_root}" ]; then
        EMCC_ENV+=("EM_BINARYEN_ROOT=${binaryen_root}")
    fi

    env "${EMCC_ENV[@]}" "${EMCC_BIN}" --version >/dev/null
}

mkdir -p "${OUT_DIR}"
require_cmd "${EMCC_BIN}"
require_cmd xxd
prepare_emcc_env

for app in clock_demo settings; do
    env "${EMCC_ENV[@]}" "${EMCC_BIN}" "${ROOT_DIR}/wasm_apps/${app}/${app}.c" \
        -O2 -s STANDALONE_WASM=1 -s INITIAL_MEMORY=131072 -s STACK_SIZE=4096 \
        -Wl,--no-entry -Wl,--export=run -o "${OUT_DIR}/${app}.wasm"
done
cp "${OUT_DIR}/clock_demo.wasm" "${OUT_DIR}/watch.wasm"

{
    printf '#pragma once\n\n#include <stdint.h>\n\n'
    xxd -i -n k_watch_demo_clock_wasm "${OUT_WASM}" | \
        sed -e 's/^unsigned char /static const uint8_t /' \
            -e '/^unsigned int k_watch_demo_clock_wasm_len = /d'
} > "${OUT_HEADER}"
printf 'Built Watch and Settings in %s; refreshed embedded Watch fallback\n' "${OUT_DIR}"
