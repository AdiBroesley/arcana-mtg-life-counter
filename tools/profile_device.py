"""Capture an on-demand 30-second ARCANA performance summary without resetting it."""
import argparse
import pathlib
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--benchmark-undo", action="store_true", help="Run the automatic live/frozen/live Undo drawing comparison")
    args = parser.parse_args()
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.2
    connection.dtr = False
    connection.rts = False
    output = bytearray()
    try:
        connection.open()
        connection.reset_input_buffer()
        connection.write(b"b\n" if args.benchmark_undo else b"p\n")
        connection.flush()
        print("Automatic Undo drawing comparison: leave the controls still until it finishes." if args.benchmark_undo else "Waiting for PROFILE START, then record 30 seconds of normal use.", flush=True)
        deadline = time.monotonic() + (60 if args.benchmark_undo else 45)
        while time.monotonic() < deadline:
            data = connection.read(4096)
            output.extend(data)
            if data:
                print(data.decode("utf-8", errors="replace"), end="", flush=True)
            if args.benchmark_undo and b"RENDER_BENCH BUSY" in output:
                print("Open a Single ARCANA game on the main life face and stop any timing capture first.", flush=True)
                return 1
            if (b"RENDER_BENCH END restored=1" if args.benchmark_undo else b"PROFILE END") in output:
                return 0
        print("No complete summary received. Check the device log and installed diagnostic firmware.", flush=True)
        return 1
    finally:
        connection.close()
        if args.output:
            args.output.write_bytes(output)


if __name__ == "__main__":
    raise SystemExit(main())
