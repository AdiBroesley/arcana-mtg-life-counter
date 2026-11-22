"""Exercise backup validation without opening a device or touching real backups."""
import importlib.util
import json
import pathlib
import tempfile
import struct
import unittest

spec = importlib.util.spec_from_file_location("knob_device", pathlib.Path(__file__).parents[1] / "tools/knob_device.py")
device = importlib.util.module_from_spec(spec)
spec.loader.exec_module(device)


class BackupTests(unittest.TestCase):
    def test_ota_layout_preserves_profile_storage(self):
        entries = [("nvs", 1, 2, 0x9000, 0x6000), ("phy_init", 1, 1, 0xF000, 0x1000),
                   ("ota_0", 0, 0x10, 0x10000, 0x370000), ("ota_1", 0, 0x11, 0x380000, 0x370000),
                   ("otadata", 1, 0, 0x6F0000, 0x2000), ("storage", 1, 0x82, 0x710000, 0x8F0000)]
        def encode(values):
            return b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size, label.encode(), 0)
                            for label, kind, subtype, offset, size in values)
        self.assertTrue(device.firmware_layout(encode(entries)))
        broken = list(entries)
        broken[-1] = ("storage", 1, 0x82, 0x720000, 0x8E0000)
        with self.assertRaises(RuntimeError):
            device.firmware_layout(encode(broken))
        with self.assertRaises(RuntimeError):
            device.firmware_layout(encode(entries + [entries[0]]))
        legacy = entries[:2] + [("factory", 0, 0, 0x10000, 0x700000), entries[-1]]
        self.assertFalse(device.firmware_layout(encode(legacy)))

    def test_backup_requires_matching_device_size_and_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "backup.bin"
            path.write_bytes(b"\xff" * device.FLASH_SIZE)
            metadata = {"chip": "esp32s3", "mac": "00:11:22:33:44:55",
                        "size": device.FLASH_SIZE, "sha256": device.digest(path), "verified": True}
            sidecar = path.with_suffix(".json")
            sidecar.write_text(json.dumps(metadata), encoding="utf-8")
            self.assertEqual(device.validate_backup(path, metadata["mac"]), path.resolve())
            with self.assertRaises(RuntimeError):
                device.validate_backup(path, "aa:bb:cc:dd:ee:ff")
            metadata["verified"] = False
            sidecar.write_text(json.dumps(metadata), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                device.validate_backup(path, metadata["mac"])
            metadata["verified"] = True
            sidecar.write_text(json.dumps(metadata), encoding="utf-8")
            with path.open("r+b") as stream:
                stream.write(b"corrupt")
            with self.assertRaises(RuntimeError):
                device.validate_backup(path, metadata["mac"])
            path.write_bytes(b"short")
            with self.assertRaises(RuntimeError):
                device.validate_backup(path, metadata["mac"])


if __name__ == "__main__":
    unittest.main()
