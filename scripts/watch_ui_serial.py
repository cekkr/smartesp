#!/usr/bin/env python3
"""Exercise real-device LVGL input and capture its rendered screen over USB."""
import argparse
from pathlib import Path
import select
import time
import os
from push_wasm_serial import SerialSession, detect_port


def lines(session, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        while b"\n" in session.buffer:
            raw, _, rest = session.buffer.partition(b"\n")
            session.buffer = bytearray(rest)
            line = raw.decode("utf-8", errors="replace").strip()
            marker = line.find("WATCH-UI ")
            yield line[marker:] if marker >= 0 else line
        ready, _, _ = select.select([session.fd], [], [], 0.1)
        if ready:
            session.buffer.extend(os.read(session.fd, 8192))


def command(session, text):
    session.pending = []
    session.send_line("WATCH-UI " + text)
    for line in lines(session, 4):
        print(line)
        if line.startswith("WATCH-UI ERROR"):
            raise RuntimeError(line)
        if line.startswith("WATCH-UI OK"):
            return
        session.pending.append(line)
    raise RuntimeError("No response to " + text)


def expect(session, marker):
    for index, line in enumerate(getattr(session, "pending", [])):
        if marker in line:
            session.pending = session.pending[index + 1:]
            return line
    session.pending = []
    for line in lines(session, 6):
        print(line)
        if "Guru Meditation" in line or "panic" in line.lower():
            raise RuntimeError(line)
        if marker in line:
            return line
    raise RuntimeError("Missing device marker: " + marker)


def snapshot(session, path):
    session.send_line("WATCH-UI SNAPSHOT")
    width = height = 0
    rows = []
    for line in lines(session, 45):
        if line.startswith("WATCH-UI FRAME "):
            width, height = map(int, line.split()[-2:])
            print(f"Receiving {width}x{height} screenshot", flush=True)
        elif line.startswith("WATCH-UI PIXELS "):
            try:
                rows.append(bytes.fromhex(line.removeprefix("WATCH-UI PIXELS ")))
            except ValueError as error:
                raise RuntimeError(f"Corrupt screenshot row {len(rows)} ({len(line)} chars): {line[:80]!r} ... {line[-100:]!r}") from error
        elif line == "WATCH-UI FRAME-END":
            break
        elif line.startswith("WATCH-UI ERROR"):
            raise RuntimeError(line)
        elif line:
            print(line, flush=True)
            if "Guru Meditation" in line or "Backtrace:" in line:
                raise RuntimeError("Device crashed during screenshot")
    if not width or len(rows) != height or any(len(row) != width * 2 for row in rows):
        raise RuntimeError(f"Incomplete screenshot: {width}x{height}, {len(rows)} rows")
    rgb = bytearray()
    for row in rows:
        for x in range(0, len(row), 2):
            pixel = row[x] | row[x + 1] << 8
            rgb.extend((((pixel >> 11) & 31) * 255 // 31,
                        ((pixel >> 5) & 63) * 255 // 63, (pixel & 31) * 255 // 31))
    Path(path).write_bytes(f"P6\n{width} {height}\n255\n".encode() + rgb)
    print("Saved " + str(path))


def smoke(session, output):
    output.mkdir(parents=True, exist_ok=True)
    # Start with the normal booted Watch face. Actual pointer hit testing is used.
    command(session, "BUTTON")
    expect(session, "apps-ui rendered")
    # Watch updates must not take navigation back after a second.
    snapshot(session, output / "apps.ppm")
    command(session, "TAP 292 299")
    expect(session, "loading wasm.settings source=sd")
    opened = expect(session, "settings-ui rendered brightness=")
    initial = int(opened.rsplit("=", 1)[1])
    snapshot(session, output / "settings.ppm")
    command(session, "TAP 120 332")
    expect(session, f"settings-ui rendered brightness={max(20, initial - 20)}")
    command(session, "TAP 287 332")
    expect(session, f"settings-ui rendered brightness={max(40, initial)}")
    # Restore the user's initial brightness, including the lower-bound case.
    if initial == 20:
        command(session, "TAP 120 332")
        expect(session, "settings-ui rendered brightness=20")
    command(session, "BUTTON")
    expect(session, "apps-ui rendered")
    command(session, "TAP 123 232")
    expect(session, "clock-ui rendered time=")
    snapshot(session, output / "watch.ppm")
    command(session, "BUTTON")
    expect(session, "apps-ui rendered")
    command(session, "BUTTON")
    expect(session, "clock-ui rendered time=")
    print("Launcher hardware smoke passed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", type=Path, default=Path("/tmp/smartesp-ui"))
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("button")
    tap = sub.add_parser("tap")
    tap.add_argument("x", type=int)
    tap.add_argument("y", type=int)
    sub.add_parser("snapshot")
    sub.add_parser("smoke")
    args = parser.parse_args()
    session = SerialSession(args.port or detect_port(), args.baud, 6)
    try:
        if args.action == "smoke":
            smoke(session, args.output)
        elif args.action == "snapshot":
            snapshot(session, args.output)
        elif args.action == "tap":
            command(session, f"TAP {args.x} {args.y}")
        else:
            command(session, "BUTTON")
    finally:
        session.close()


if __name__ == "__main__":
    main()
