"""Read-only identification/backup and guarded ARCANA installation. Requires esptool 4.9."""
import argparse
import datetime
import hashlib
import json
import pathlib
import re
import subprocess
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FLASH_SIZE = 16 * 1024 * 1024


def esptool(port, *args, capture=False, reset=True):
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port,
               "--baud", "460800", "--after", "hard_reset" if reset else "no_reset", *map(str, args)]
    result = subprocess.run(command, check=True, text=True,
                            stdout=subprocess.PIPE if capture else None,
                            stderr=subprocess.STDOUT if capture else None)
    return result.stdout or ""


def identify(port):
    information = esptool(port, "flash_id", capture=True)
    print(information)
    match = re.search(r"MAC:\s*([0-9a-f:]{17})", information, re.I)
    if not match or not re.search(r"Detected flash size:\s*16MB", information):
        raise RuntimeError("Expected an ESP32-S3 with 16 MB flash; refusing to write.")
    security = esptool(port, "get_security_info", capture=True)
    print(security)
    if "Secure Boot: Disabled" not in security or "Flash Encryption: Disabled" not in security:
        raise RuntimeError("Cannot confirm unsecured flash; no writes will be attempted.")
    return match[1].lower()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def backup(port):
    mac = identify(port)
    directory = ROOT / "backups"
    directory.mkdir(exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    path = directory / f"jc3636k718-{mac.replace(':', '')}-{stamp}.bin"
    if path.exists():
        raise RuntimeError("Backup already exists; refusing to overwrite it.")
    esptool(port, "read_flash", "0", hex(FLASH_SIZE), path)
    if path.stat().st_size != FLASH_SIZE:
        raise RuntimeError("Incomplete flash backup.")
    # Compare the backup with flash before allowing it to authorize an installation.
    esptool(port, "verify_flash", "0", path)
    metadata = {"chip": "esp32s3", "mac": mac, "size": FLASH_SIZE,
                "sha256": digest(path), "verified": True, "created_utc": stamp}
    path.with_suffix(".json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(f"Verified factory backup: {path}")


def validate_backup(path, mac):
    path = path.resolve(strict=True)
    metadata = json.loads(path.with_suffix(".json").read_text(encoding="utf-8"))
    if (metadata.get("chip") != "esp32s3" or metadata.get("mac") != mac
            or metadata.get("verified") is not True or metadata.get("size") != FLASH_SIZE
            or path.stat().st_size != FLASH_SIZE or metadata.get("sha256") != digest(path)):
        raise RuntimeError("A complete, verified backup of this exact device is required.")
    return path


def firmware_layout(table):
    entries = {}
    for start in range(0, len(table) - 31, 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from("<HBBII16sI", table, start)
        if magic != 0x50AA:
            break
        name = label.rstrip(b"\x00").decode("ascii")
        if name in entries:
            raise RuntimeError("Duplicate partition")
        entries[name] = (kind, subtype, offset, size)
    fixed = {"nvs": (1, 2, 0x9000, 0x6000), "phy_init": (1, 1, 0xF000, 0x1000),
             "storage": (1, 0x82, 0x710000, 0x8F0000)}
    ota = {**fixed, "ota_0": (0, 0x10, 0x10000, 0x370000),
           "ota_1": (0, 0x11, 0x380000, 0x370000), "otadata": (1, 0, 0x6F0000, 0x2000)}
    legacy = {**fixed, "factory": (0, 0, 0x10000, 0x700000)}
    if entries == ota:
        return True
    if entries == legacy:
        return False
    raise RuntimeError("Unexpected partition layout; profile storage must stay unchanged")


def install(port, backup_path, initialize_storage):
    mac = identify(port)
    validate_backup(backup_path, mac)
    build = ROOT / "build-esp32/jc3636k718"
    manifest = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
    ota = firmware_layout((build / "partitions.bin").read_bytes())
    expected_offsets = {0x0, 0x8000, 0x10000}
    offsets = set(int(offset, 0) for offset in manifest["flash_files"])
    if (offsets not in (expected_offsets, expected_offsets | ({0x6F0000} if ota else set()))
            or manifest.get("extra_esptool_args", {}).get("chip") != "esp32s3"
            or manifest.get("flash_settings", {}).get("flash_size") != "16MB"):
        raise RuntimeError("Unexpected firmware chip, flash size or partition layout.")
    images = []
    # PlatformIO's SCons builder emits these names; IDF's JSON uses CMake names.
    platformio_images = {0x0: "bootloader.bin", 0x8000: "partitions.bin", 0x10000: "firmware.bin"}
    if ota:
        platformio_images[0x6F0000] = "ota_data_initial.bin"
    for offset, name in manifest["flash_files"].items():
        address = int(offset, 0)
        if address not in platformio_images:
            raise RuntimeError("Unexpected flash image offset.")
        path = (build / platformio_images[address]).resolve(strict=True)
        if not path.is_relative_to(build.resolve()):
            raise RuntimeError("Unexpected firmware artifact path.")
        limit = {0x0: 0x8000, 0x8000: 0x9000, 0x10000: 0x380000 if ota else 0x710000, 0x6F0000: 0x6F2000}[address]
        if path.stat().st_size == 0 or address + path.stat().st_size > limit:
            raise RuntimeError("Firmware artifact exceeds the application region.")
        images += [offset, path]
    if not images:
        raise RuntimeError("No firmware artifacts found.")
    # Explicit first-install option only. Subsequent updates preserve the device profile and existing storage.
    if initialize_storage:
        esptool(port, "erase_region", "0x9000", "0x7000", reset=False)
        esptool(port, "erase_region", "0x710000", "0x8f0000", reset=False)
    if ota and 0x6F0000 not in offsets:
        # Select the USB-installed slot without touching NVS or device-name storage.
        esptool(port, "erase_region", "0x6f0000", "0x2000", reset=False)
    esptool(port, "write_flash", "--flash_size", "16MB", *images, reset=False)
    pairs=list(zip(images[::2], images[1::2]))
    for index,(offset, path) in enumerate(pairs):
        esptool(port, "verify_flash", offset, path, reset=index==len(pairs)-1)
    print("Firmware written and verified. Restart the knob and check screen, touch and rotation.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["ports", "inspect", "backup", "flash"])
    parser.add_argument("--port")
    parser.add_argument("--backup", type=pathlib.Path)
    parser.add_argument("--initialize-storage", action="store_true",
                        help="First installation only: erase ARCANA storage after validating factory backup")
    args = parser.parse_args()
    if args.action == "ports":
        from serial.tools.list_ports import comports
        for port in comports():
            print(port.device, port.description, port.hwid)
        return
    if not args.port:
        parser.error("--port is required")
    if args.action == "inspect":
        identify(args.port)
    elif args.action == "backup":
        backup(args.port)
    else:
        if not args.backup:
            parser.error("--backup must name the verified factory backup")
        install(args.port, args.backup, args.initialize_storage)


if __name__ == "__main__":
    main()
