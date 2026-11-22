"""Read a bounded serial log without toggling the device's boot/reset control lines."""
import argparse
import pathlib
import time

import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True)
parser.add_argument("--seconds", type=float, default=20)
parser.add_argument("--output", type=pathlib.Path)
parser.add_argument("--reset", action="store_true", help="Restart once to capture a complete boot log")
args = parser.parse_args()
if not 0 < args.seconds <= 60:
    parser.error("--seconds must be between 0 and 60")
connection = serial.Serial()
connection.port = args.port
connection.baudrate = 115200
connection.timeout = 0.2
connection.dtr = False
connection.rts = False
connection.open()
if args.reset:
    from esptool.reset import HardReset
    HardReset(connection, uses_usb=True)()
output = bytearray()
try:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        data = connection.read(4096)
        output.extend(data)
        if data:
            print(data.decode("utf-8", errors="replace"), end="", flush=True)
finally:
    connection.close()
if args.output:
    args.output.write_bytes(output)
