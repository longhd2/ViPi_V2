from __future__ import annotations

import csv
import hashlib
import json
import os
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT.parent
PARTITIONS = ROOT / "partitions.csv"
RELEASE = ROOT / "release"


def number(value: str) -> int:
    value = value.strip()
    return int(value, 16) if value.lower().startswith("0x") else int(value)


def partitions():
    result = {}

    with PARTITIONS.open(
        "r",
        encoding="utf-8",
        newline=""
    ) as f:
        rows = csv.reader(
            line for line in f
            if line.strip() and not line.lstrip().startswith("#")
        )

        for row in rows:
            if len(row) < 5:
                continue

            name = row[0].strip()

            result[name] = {
                "type": row[1].strip(),
                "subtype": row[2].strip(),
                "offset": number(row[3]),
                "size": number(row[4]),
            }

    return result


def sha256(path: Path) -> str:
    h = hashlib.sha256()

    with path.open("rb") as f:
        while True:
            block = f.read(1024 * 1024)
            if not block:
                break
            h.update(block)

    return h.hexdigest()


def git_commit():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=PROJECT,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except Exception:
        return "unknown"


def main():
    p = partitions()

    for required in ("ota_0", "ota_1", "storage"):
        if required not in p:
            raise RuntimeError(
                f"Khong tim thay partition {required}"
            )

    ota0 = p["ota_0"]
    ota1 = p["ota_1"]
    storage = p["storage"]

    if ota0["size"] != ota1["size"]:
        raise RuntimeError(
            "ota_0 va ota_1 khong cung kich thuoc"
        )

    app = ROOT / "VIPI_V2_CLIENT_APP.bin"
    full = ROOT / "VIPI_V2_CLIENT_FULL.bin"
    sounds = ROOT / "VIPI_V2_SOUNDS.bin"

    for path in (app, full, sounds):
        if not path.exists():
            raise RuntimeError(
                f"Thieu file: {path.name}"
            )

    app_size = app.stat().st_size
    sound_size = sounds.stat().st_size

    if app_size > ota0["size"]:
        raise RuntimeError(
            f"Firmware OTA vuot partition: "
            f"{app_size} > {ota0['size']}"
        )

    if sound_size > storage["size"]:
        raise RuntimeError(
            f"Sound Pack vuot partition: "
            f"{sound_size} > {storage['size']}"
        )

    version = (
        ROOT / "VERSION"
    ).read_text(
        encoding="utf-8"
    ).strip()

    build_meta_path = ROOT / "generated" / "build.json"

    if not build_meta_path.exists():
        raise RuntimeError(
            "Thieu generated/build.json"
        )

    build_meta = json.loads(
        build_meta_path.read_text(
            encoding="utf-8"
        )
    )

    commit = build_meta.get(
        "git_commit",
        git_commit()
    )
    build_time = build_meta.get(
        "build_time",
        "N/A"
    )
    build_id = build_meta.get(
        "build_id",
        "N/A"
    )

    RELEASE.mkdir(parents=True, exist_ok=True)

    names = {
        app: "VIPI_V2_CLIENT_APP.bin",
        full: "VIPI_V2_CLIENT_FULL.bin",
        sounds: "VIPI_V2_SOUNDS.bin",
    }

    files = {}

    for source, name in names.items():
        dest = RELEASE / name
        shutil.copy2(source, dest)

        files[name] = {
            "size": dest.stat().st_size,
            "sha256": sha256(dest),
        }

    ota_url = (
        "https://github.com/"
        "longhd2/ViPi-ESP32-Firmware/"
        f"releases/download/v{version}/"
        "VIPI_V2_CLIENT_APP.bin"
    )

    manifest = {
        "product": "ViPi ESP32 Client",
        "channel": "stable",
        "version": version,
        "build_id": build_id,
        "build_time": build_time,
        "git_commit": commit,
        "ota": {
            "url": ota_url,
            "size": files["VIPI_V2_CLIENT_APP.bin"]["size"],
            "sha256": files["VIPI_V2_CLIENT_APP.bin"]["sha256"],
        },
        "files": files,
        "partition": {
            "ota_size": ota0["size"],
            "storage_offset": storage["offset"],
            "storage_size": storage["size"],
        },
    }

    (RELEASE / "manifest.json").write_text(
        json.dumps(
            manifest,
            ensure_ascii=False,
            indent=2
        ) + "\n",
        encoding="utf-8",
    )

    (RELEASE / "latest.json").write_text(
        json.dumps(
            manifest,
            ensure_ascii=False,
            indent=2
        ) + "\n",
        encoding="utf-8",
    )

    generated = ROOT / "generated"
    generated.mkdir(parents=True, exist_ok=True)

    (generated / "version.h").write_text(
        "#pragma once\n"
        f'#define VIPI_FW_VERSION "{version}"\n'
        f'#define VIPI_BUILD_ID "{build_id}"\n'
        f'#define VIPI_BUILD_TIME "{build_time}"\n',
        encoding="utf-8",
    )

    print("VERSION       :", version)
    print("BUILD ID      :", build_id)
    print("OTA SIZE      :", app_size)
    print("OTA MAX       :", ota0["size"])
    print(
        "OTA FREE      :",
        ota0["size"] - app_size
    )
    print(
        "STORAGE OFFSET:",
        f"0x{storage['offset']:X}"
    )
    print("STORAGE SIZE  :", storage["size"])
    print("MANIFEST      :", RELEASE / "latest.json")
    print("RELEASE BUILD : PASS")


if __name__ == "__main__":
    main()
