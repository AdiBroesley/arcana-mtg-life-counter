"""Inspect battery/ring state or briefly force dim red over USB to test the LED driver."""
import argparse
import pathlib
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--action", choices=("status", "dma", "rmt", "auto"), default="status")
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    testing = args.action in ("dma", "rmt")
    command = {"status": b"d\n", "dma": b"t\n", "rmt": b"u\n", "auto": b"x\nd\n"}[args.action]
    ending = b"RING TEST RESTORED" if testing else b"RING STATUS END"
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.2
    connection.dtr = False
    connection.rts = False
    output = bytearray()
    complete = False
    try:
        connection.open()
        connection.reset_input_buffer()
        connection.write(command)
        connection.flush()
        if testing:
            print("Watch the ring with USB CONNECTED: dim red for 15 seconds, then automatic mode resumes.", flush=True)
        deadline = time.monotonic() + (25 if testing else 8)
        while time.monotonic() < deadline:
            data = connection.read(4096)
            output.extend(data)
            if data:
                print(data.decode("utf-8", errors="replace"), end="", flush=True)
            if ending in output:
                complete = True
                if b"failed=1" in output or b"RGB update failed" in output:
                    print("The device reported an LED driver error; inspect the diagnostic log.", flush=True)
                    return 1
                if b"RING TEST START" in output:
                    print("Test interval completed. Visual confirmation is still needed.", flush=True)
                return 0
        print("No complete diagnostic response received; check the firmware and serial connection.", flush=True)
        return 1
    finally:
        if testing and not complete and connection.is_open:
            try:
                connection.write(b"x\n")
                connection.flush()
            except serial.SerialException:
                pass  # The device also restores automatic mode on its own timer.
        connection.close()
        if args.output:
            args.output.write_bytes(output)


if __name__ == "__main__":
    raise SystemExit(main())
