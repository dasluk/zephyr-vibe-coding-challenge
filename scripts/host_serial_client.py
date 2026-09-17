#!/usr/bin/env python3
"""Reference host-side client for the board<->host serial protocol.

Opens a controller board's serial port, prints parsed HELLO/BTN/GESTURE/
AXIS/PONG events as they arrive, and can send BUZZ/PING commands from stdin.
See specs/communication.spec.md and intent.md for the protocol.

This is intentionally a small, dependency-light reference/starting point
for whoever builds the actual host game -- not a finished client.

Usage:
    python3 scripts/host_serial_client.py /dev/ttyACM0
    python3 scripts/host_serial_client.py /dev/ttyACM0 --baud 115200

Once running, type commands on stdin and press enter:
    buzz 440 200      -> sends "BUZZ 440 200"
    ping              -> sends "PING <current_ms>"
    reset             -> sends "RESET"
    <anything else>   -> sent verbatim as a raw line (advanced/debug use)

Requires pyserial (`pip install pyserial`).
"""
from __future__ import annotations

import argparse
import sys
import threading
import time
from dataclasses import dataclass
from typing import Optional

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover - convenience error for humans
    sys.stderr.write(
        "error: pyserial is required (pip install pyserial)\n"
    )
    raise


@dataclass
class ParsedLine:
    kind: str
    fields: list[str]
    raw: str


def parse_line(raw: str) -> Optional[ParsedLine]:
    """Mirrors comms_decode-style parsing, but for board->host lines.

    Returns None for a blank line. Any line whose first token isn't one of
    the known keywords is still returned, tagged "UNKNOWN", rather than
    dropped -- useful for spotting protocol drift or stray log output that
    leaked onto the link.
    """
    line = raw.strip("\r\n")
    if not line.strip():
        return None
    parts = line.split()
    kind = parts[0] if parts else "UNKNOWN"
    known = {"HELLO", "BTN", "GESTURE", "AXIS", "PONG"}
    if kind not in known:
        kind = "UNKNOWN"
    return ParsedLine(kind=kind, fields=parts[1:], raw=line)


def format_event(p: ParsedLine) -> str:
    if p.kind == "HELLO" and len(p.fields) >= 2:
        return f"HELLO  controller_id={p.fields[0]} fw_version={p.fields[1]}"
    if p.kind == "BTN" and len(p.fields) >= 3:
        return f"BTN    id={p.fields[0]} state={p.fields[1]} t_ms={p.fields[2]}"
    if p.kind == "GESTURE" and len(p.fields) >= 3:
        return f"GESTURE name={p.fields[0]} confidence={p.fields[1]} t_ms={p.fields[2]}"
    if p.kind == "AXIS" and len(p.fields) >= 3:
        return f"AXIS   name={p.fields[0]} value={p.fields[1]} t_ms={p.fields[2]}"
    if p.kind == "PONG" and len(p.fields) >= 1:
        return f"PONG   t_ms={p.fields[0]}"
    return f"?      {p.raw!r}"


def reader_thread(ser: "serial.Serial", stop: threading.Event) -> None:
    buf = b""
    while not stop.is_set():
        try:
            chunk = ser.read(ser.in_waiting or 1)
        except serial.SerialException as exc:
            print(f"[serial read error: {exc}]", file=sys.stderr)
            return
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            raw_line, buf = buf.split(b"\n", 1)
            text = raw_line.decode("ascii", errors="replace")
            parsed = parse_line(text)
            if parsed is None:
                continue
            print(format_event(parsed))


def send_line(ser: "serial.Serial", line: str) -> None:
    ser.write((line + "\n").encode("ascii"))
    ser.flush()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("port", help="serial port, e.g. /dev/ttyACM0 or COM5")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    stop = threading.Event()
    t = threading.Thread(target=reader_thread, args=(ser, stop), daemon=True)
    t.start()

    print(f"Connected to {args.port} @ {args.baud}. Commands: buzz <hz> <ms> | ping | reset | <raw line>")
    try:
        for input_line in sys.stdin:
            cmd = input_line.strip()
            if not cmd:
                continue
            lower = cmd.lower()
            if lower.startswith("buzz"):
                parts = cmd.split()
                if len(parts) != 3:
                    print("usage: buzz <freq_hz> <duration_ms>")
                    continue
                send_line(ser, f"BUZZ {parts[1]} {parts[2]}")
            elif lower == "ping":
                send_line(ser, f"PING {int(time.time() * 1000)}")
            elif lower == "reset":
                send_line(ser, "RESET")
            else:
                # Advanced/debug: send whatever was typed verbatim.
                send_line(ser, cmd)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        ser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
