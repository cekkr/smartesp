#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
IDF_PATH_DEFAULT="/Users/richard/esp-idf/esp-idf-v6.1/v6.1/esp-idf"
IDF_ACTIVATE_SCRIPT="${IDF_ACTIVATE_SCRIPT:-}"
IDF_PATH_INPUT="${IDF_PATH:-$IDF_PATH_DEFAULT}"
BUILD_DIR="${BUILD_DIR:-build-test}"
TARGET="${TARGET:-esp32s3}"
PORT="${PORT:-}"
BAUD="${BAUD:-921600}"
MONITOR_SECONDS="${MONITOR_SECONDS:-20}"
SCENARIO="${SCENARIO:-smoke-ui}"
DO_FLASH=0
DO_MONITOR=0
DO_CLEAN=1
SHOW_HELP=0
EXPECT_LOGS=""

usage() {
    cat <<EOF
Usage: $0 [options]

Compile validation:
  $0

Compile + flash + serial smoke validation:
  $0 --flash --monitor --port /dev/cu.usbmodemXXXX

Options:
  --idf-path PATH         ESP-IDF root. Default: $IDF_PATH_DEFAULT
  --build-dir DIR         Build directory. Default: $BUILD_DIR
  --target CHIP           IDF target. Default: $TARGET
  --port DEVICE           Serial device for flash/monitor
  --baud RATE             Serial baud. Default: $BAUD
  --flash                 Flash firmware after successful build
  --monitor               Capture serial logs and assert expected markers
  --monitor-seconds N     Seconds to observe serial output. Default: $MONITOR_SECONDS
  --scenario NAME         Built-in assertion set. Default: $SCENARIO
  --expect-log TEXT       Additional log line fragment to require
  --no-clean              Skip idf.py fullclean
  --help                  Show this help

Built-in scenarios:
  smoke-ui
    Requires the firmware to emit:
    - boot-ui stage="display ready" progress=10
    - boot-ui stage="launching clock" progress=100
    - clock demo wasm.demo.clock time=
    - clock-ui rendered time=
    - SmartOS started
EOF
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "missing command: $1"
}

run_in_idf_env() {
    bash -lc '
        set -eu
        IDF_ROOT=$1
        PROJECT_ROOT=$2
        ACTIVATE_SCRIPT=$3
        shift 3
        if [ -f "$ACTIVATE_SCRIPT" ]; then
            # EIM activation scripts inspect positional args and may return a
            # nonzero completion status even after a successful activation.
            set +u
            . "$ACTIVATE_SCRIPT" >/dev/null || true
            set -u
        else
            . "$IDF_ROOT/export.sh" >/dev/null
        fi
        cd "$PROJECT_ROOT"
        "$@"
    ' sh "$IDF_PATH_INPUT" "$ROOT_DIR" "$IDF_ACTIVATE_SCRIPT" "$@"
}

project_target_matches() {
    [ -f "$ROOT_DIR/sdkconfig" ] && grep -F "CONFIG_IDF_TARGET=\"$TARGET\"" "$ROOT_DIR/sdkconfig" >/dev/null 2>&1
}

append_expect_log() {
    if [ -z "$EXPECT_LOGS" ]; then
        EXPECT_LOGS=$1
    else
        EXPECT_LOGS=$(printf '%s\n%s' "$EXPECT_LOGS" "$1")
    fi
}

detect_port() {
    for candidate in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/tty.usbmodem* /dev/tty.usbserial*; do
        if [ -e "$candidate" ]; then
            PORT=$candidate
            return 0
        fi
    done
    return 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --idf-path)
            [ $# -ge 2 ] || fail "--idf-path requires a value"
            IDF_PATH_INPUT=$2
            shift 2
            ;;
        --build-dir)
            [ $# -ge 2 ] || fail "--build-dir requires a value"
            BUILD_DIR=$2
            shift 2
            ;;
        --target)
            [ $# -ge 2 ] || fail "--target requires a value"
            TARGET=$2
            shift 2
            ;;
        --port)
            [ $# -ge 2 ] || fail "--port requires a value"
            PORT=$2
            shift 2
            ;;
        --baud)
            [ $# -ge 2 ] || fail "--baud requires a value"
            BAUD=$2
            shift 2
            ;;
        --flash)
            DO_FLASH=1
            shift
            ;;
        --monitor)
            DO_MONITOR=1
            shift
            ;;
        --monitor-seconds)
            [ $# -ge 2 ] || fail "--monitor-seconds requires a value"
            MONITOR_SECONDS=$2
            shift 2
            ;;
        --scenario)
            [ $# -ge 2 ] || fail "--scenario requires a value"
            SCENARIO=$2
            shift 2
            ;;
        --expect-log)
            [ $# -ge 2 ] || fail "--expect-log requires a value"
            append_expect_log "$2"
            shift 2
            ;;
        --no-clean)
            DO_CLEAN=0
            shift
            ;;
        --help)
            SHOW_HELP=1
            shift
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

if [ -z "$IDF_ACTIVATE_SCRIPT" ] && [ "$IDF_PATH_INPUT" = "$IDF_PATH_DEFAULT" ] &&
   [ -f /Users/richard/.espressif/tools/activate_idf_v6.1.sh ]; then
    IDF_ACTIVATE_SCRIPT=/Users/richard/.espressif/tools/activate_idf_v6.1.sh
fi

if [ "$SHOW_HELP" -eq 1 ]; then
    usage
    exit 0
fi

require_cmd python3
require_cmd bash
require_cmd git

[ -d "$IDF_PATH_INPUT" ] || fail "ESP-IDF path not found: $IDF_PATH_INPUT"
[ -f "$IDF_PATH_INPUT/export.sh" ] || fail "missing export.sh under: $IDF_PATH_INPUT"
[ -e "$ROOT_DIR/external/fayasm/.git" ] || fail "fayasm submodule is not initialized"

case "$SCENARIO" in
    smoke-ui)
        append_expect_log 'boot-ui stage="display ready" progress=10'
        append_expect_log 'boot-ui stage="launching clock" progress=100'
        append_expect_log 'clock demo wasm.demo.clock time='
        append_expect_log 'clock-ui rendered time='
        append_expect_log 'SmartOS started'
        ;;
    none)
        ;;
    *)
        fail "unknown scenario: $SCENARIO"
        ;;
esac

if [ "$DO_FLASH" -eq 1 ] || [ "$DO_MONITOR" -eq 1 ]; then
    if [ -z "$PORT" ]; then
        detect_port || fail "no serial port detected; pass --port explicitly"
    fi
fi

printf 'Using ESP-IDF: %s\n' "$IDF_PATH_INPUT"
printf 'Project root: %s\n' "$ROOT_DIR"
printf 'Build dir: %s\n' "$BUILD_DIR"
printf 'Target: %s\n' "$TARGET"
if [ -n "$PORT" ]; then
    printf 'Serial port: %s\n' "$PORT"
fi

MONITOR_LOG=$(mktemp)
cleanup() {
    rm -f "$MONITOR_LOG"
}
trap cleanup EXIT INT TERM

cd "$ROOT_DIR"

git submodule update --init --recursive

if [ "$DO_CLEAN" -eq 1 ]; then
    run_in_idf_env idf.py -B "$BUILD_DIR" fullclean >/dev/null 2>&1 || true
    run_in_idf_env idf.py -B "$BUILD_DIR" set-target "$TARGET"
elif project_target_matches; then
    run_in_idf_env idf.py -B "$BUILD_DIR" reconfigure
else
    run_in_idf_env idf.py -B "$BUILD_DIR" set-target "$TARGET"
fi

run_in_idf_env idf.py -B "$BUILD_DIR" build

if [ "$DO_FLASH" -eq 1 ]; then
    run_in_idf_env idf.py -B "$BUILD_DIR" -p "$PORT" -b "$BAUD" flash
fi

if [ "$DO_MONITOR" -eq 1 ]; then
    run_in_idf_env python3 - "$PORT" "$TARGET" "$BAUD" "$MONITOR_SECONDS" "$MONITOR_LOG" <<'PY'
import os
import select
import subprocess
import sys
import termios
import time

port, target, baud, seconds, log_path = sys.argv[1:]
seconds = int(seconds)
baud = int(baud)

subprocess.run(
    [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        target,
        "-p",
        port,
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        "chip_id",
    ],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    check=False,
)

fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK)
try:
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[3] = 0
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        speed_attr = f"B{baud}"
        speed = getattr(termios, speed_attr, None)
        if speed is not None:
            attrs[4] = speed
            attrs[5] = speed
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
    except Exception:
        pass

    deadline = time.time() + seconds
    with open(log_path, "w", encoding="utf-8", errors="replace") as log:
        while time.time() < deadline:
            ready, _, _ = select.select([fd], [], [], 1.0)
            if not ready:
                continue
            chunk = os.read(fd, 4096)
            if not chunk:
                continue
            log.write(chunk.decode("utf-8", errors="replace"))
            log.flush()
finally:
    os.close(fd)
PY

    printf '\nCaptured monitor log:\n'
    sed -n '1,220p' "$MONITOR_LOG"

    if [ -n "$EXPECT_LOGS" ]; then
        printf '%s\n' "$EXPECT_LOGS" | while IFS= read -r expected; do
            [ -n "$expected" ] || continue
            if ! grep -F -- "$expected" "$MONITOR_LOG" >/dev/null 2>&1; then
                fail "missing expected monitor log: $expected"
            fi
        done
    fi
fi

printf '\nSmartOS validation succeeded.\n'
