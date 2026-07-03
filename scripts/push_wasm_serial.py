#!/usr/bin/python3

import argparse
import glob
import os
import select
import sys
import termios
import time


BAUD_TO_TERM = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}

for rate_name in ("B230400", "B460800", "B921600"):
    value = getattr(termios, rate_name, None)
    if value is not None:
        BAUD_TO_TERM[int(rate_name[1:])] = value


def detect_port() -> str:
    for pattern in (
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/tty.usbmodem*",
        "/dev/tty.usbserial*",
    ):
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    raise SystemExit("no serial port detected; pass --port explicitly")


def configure_port(fd: int, baud: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
    attrs[3] = 0
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    speed = BAUD_TO_TERM.get(baud)
    if speed is None:
        raise SystemExit(f"unsupported baud rate: {baud}")
    attrs[4] = speed
    attrs[5] = speed
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


class SerialSession:
    def __init__(self, port: str, baud: int, timeout: float) -> None:
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.timeout = timeout
        self.buffer = bytearray()
        configure_port(self.fd, baud)

    def close(self) -> None:
        os.close(self.fd)

    def send_line(self, line: str) -> None:
        os.write(self.fd, line.encode("ascii") + b"\n")

    def read_until(self, prefixes: tuple[str, ...]) -> str:
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.25)
            if not ready:
                continue
            chunk = os.read(self.fd, 4096)
            if not chunk:
                continue
            self.buffer.extend(chunk)
            while b"\n" in self.buffer:
                raw, _, rest = self.buffer.partition(b"\n")
                self.buffer = bytearray(rest)
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                print(f"< {line}")
                if line.startswith("WASM-UPLOAD ERROR"):
                    raise SystemExit(line)
                if any(line.startswith(prefix) for prefix in prefixes):
                    return line
        raise SystemExit(f"timeout waiting for {prefixes}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Upload an external WASM app over USB serial.")
    parser.add_argument("wasm", help="Path to the wasm file to upload")
    parser.add_argument("--app", required=True, help="App slug under /sdcard/apps/<app>/app.wasm")
    parser.add_argument("--port", help="Serial device, e.g. /dev/cu.usbmodem101")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--chunk-bytes", type=int, default=96, help="Raw bytes per upload chunk (default: 96)")
    parser.add_argument("--timeout", type=float, default=6.0, help="Seconds to wait for each uploader response")
    args = parser.parse_args()

    wasm_path = os.path.abspath(args.wasm)
    if not os.path.isfile(wasm_path):
        raise SystemExit(f"missing wasm file: {wasm_path}")
    if args.chunk_bytes <= 0 or args.chunk_bytes > 96:
        raise SystemExit("--chunk-bytes must be between 1 and 96")

    port = args.port or detect_port()
    payload = open(wasm_path, "rb").read()

    print(f"Using port: {port}")
    print(f"WASM file: {wasm_path}")
    print(f"App slug: {args.app}")
    print(f"Bytes: {len(payload)}")

    session = SerialSession(port, args.baud, args.timeout)
    try:
        session.send_line("WASM-UPLOAD PING")
        session.read_until(("WASM-UPLOAD PONG", "WASM-UPLOAD INFO"))

        session.send_line(f"WASM-UPLOAD BEGIN {args.app} {len(payload)}")
        session.read_until(("WASM-UPLOAD READY",))

        sent = 0
        while sent < len(payload):
            chunk = payload[sent : sent + args.chunk_bytes]
            session.send_line(f"WASM-UPLOAD DATA {chunk.hex()}")
            session.read_until(("WASM-UPLOAD ACK",))
            sent += len(chunk)

        session.send_line("WASM-UPLOAD END")
        session.read_until(("WASM-UPLOAD OK",))
    finally:
        session.close()

    print("Upload complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
