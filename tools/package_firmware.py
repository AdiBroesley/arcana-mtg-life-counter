"""Prepare verified GitHub Release assets from an already built ARCANA image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def package(destination, tag=None):
    info = (ROOT / "src/core/AppInfo.hpp").read_text(encoding="utf-8")
    version = re.search(r'version="([0-9]+\.[0-9]+\.[0-9]+)"', info)[1]
    if tag and tag != "v" + version:
        raise ValueError("Release tag must match the source version")
    build = ROOT / "build-esp32/jc3636k718"
    image = build / "firmware.bin"
    # The app descriptor version is at image header + segment header + 16 bytes.
    data = image.read_bytes()
    embedded = data[48:80].split(b"\0", 1)[0].decode("ascii")
    project = data[80:112].split(b"\0", 1)[0].decode("ascii")
    if embedded != version or project != "arcana" or len(data) > 0x370000:
        raise ValueError("Built image version, project or size does not match this release")
    destination.mkdir(parents=True, exist_ok=True)
    name = "arcana-jc3636k718.bin"
    shutil.copy2(image, destination / name)
    digest = hashlib.sha256(data).hexdigest()
    (destination / "SHA256SUMS").write_text(f"{digest}  {name}\n", encoding="ascii")
    manifest = {"version": version, "board": "jc3636k718", "layout": "arcana-ota-v1",
                "size": len(data), "sha256": digest, "filename": name}
    (destination / "arcana-jc3636k718.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/release-assets")
    parser.add_argument("--tag")
    args = parser.parse_args()
    package(args.output, args.tag)
