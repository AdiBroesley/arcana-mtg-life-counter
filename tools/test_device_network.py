"""Test two ARCANA radios over USB, using isolated RAM-only diagnostic matches."""
import argparse
import pathlib
import re
import time

import serial


class Device:
    def __init__(self, port, log):
        self.port, self.log, self.text = port, log, ""
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.1)
        self.serial.dtr = self.serial.rts = False
        self.serial.port = port
        self.serial.open()

    def send(self, command):
        self.serial.write(command.encode("ascii"))
        self.serial.flush()

    def read(self, seconds=0.5):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            line = self.serial.readline().decode("utf-8", errors="replace")
            if line:
                self.log.write(f"[{self.port}] {line}")
                self.log.flush()
                self.text += line
                if "Guru Meditation" in line or "abort() was called" in line:
                    raise RuntimeError(f"{self.port}: device crashed; see log")

    def command(self, command):
        self.text = ""
        self.send(command)
        self.wait(lambda s: f"NETTEST command={command} ok=1" in s, 8)

    def wait(self, predicate, timeout=20):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.read()
            if predicate(self.text):
                return
            if "NETTEST busy:" in self.text or re.search(r"NETTEST command=\w ok=0", self.text):
                raise RuntimeError(f"{self.port}: diagnostic command rejected; see log")
        raise RuntimeError(f"{self.port}: network test timed out; see log")

    def status_until(self, predicate, timeout=25):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.text = ""
            self.send("N")
            self.read(0.8)
            if predicate(self.text):
                return
        raise RuntimeError(f"{self.port}: expected network state not reached; see log")

    def close(self):
        try:
            self.send("Q")
            self.read()
        finally:
            self.serial.close()


def run(host, guest):
    host.send("Q")
    guest.send("Q")
    host.read()
    guest.read()
    host.command("H")
    host.command("S")
    host.command("D")  # Start before a guest arrives, at 39 life.
    guest.command("J")
    guest.status_until(lambda s: "NETTEST lobby name=USBHOST " in s)
    guest.command("K")
    guest.status_until(lambda s: "attached=1 connected=1 started=1 players=2" in s)
    guest.command("A")
    host.status_until(lambda s: "player name=USBJOIN seat=2 life=41 online=1" in s)
    guest.status_until(lambda s: "player name=USBHOST seat=1 life=39 online=1" in s)
    print("PASS: Wi-Fi scan, DHCP, late join, roster, and life synchronization in both directions")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-port", required=True)
    parser.add_argument("--guest-port", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.host_port.upper() == args.guest_port.upper():
        parser.error("Two distinct connected devices are required")
    devices = []
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as log:
        try:
            for port in (args.host_port, args.guest_port):
                devices.append(Device(port, log))
            run(*devices)
        finally:
            for device in reversed(devices):
                device.close()


if __name__ == "__main__":
    main()
