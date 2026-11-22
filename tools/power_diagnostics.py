"""Read ARCANA sleep counters and battery diagnostics without restarting it.

To include actual light sleep, leave the switched-on device unplugged for
30 seconds first, then reconnect and run this before its 80-second battery
sample history expires. USB deliberately inhibits sleep for reliable access.
"""
import argparse
from pathlib import Path
import time

import serial


def capture(port, output):
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    connection.dtr = connection.rts = False
    connection.port = port
    connection.open()
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with output.open("w", encoding="utf-8") as log:
            for command, marker in ((b"e\n", "POWER END"), (b"i\n", "RING STATUS END")):
                connection.write(command)
                connection.flush()
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline:
                    line = connection.readline().decode("utf-8", errors="replace")
                    if line:
                        log.write(line)
                        log.flush()
                        print(line, end="", flush=True)
                    if "Guru Meditation" in line or "abort() was called" in line:
                        raise RuntimeError("Device crashed; see the capture log")
                    if marker in line:
                        break
                else:
                    raise RuntimeError("Power diagnostics timed out; requires the power firmware")
    finally:
        connection.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    capture(args.port, args.output)
