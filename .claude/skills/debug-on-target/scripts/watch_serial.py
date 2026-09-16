#!/usr/bin/env python3
"""Read-only serial console for the board's /dev/ttyBRIDGE0.

Prints incoming bytes as they arrive. Safe to run in the background or
non-interactively - unlike a read/write monitor, there's no stdin loop that
can hit EOF and exit early.

Usage: python3 watch_serial.py [port] [baud]
"""
import sys

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyBRIDGE0"
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

ser = serial.Serial(port, baud, timeout=1)
print(f"connected to {port} @ {baud}", file=sys.stderr)

try:
    while True:
        data = ser.read(4096)
        if data:
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
except KeyboardInterrupt:
    pass
finally:
    ser.close()
