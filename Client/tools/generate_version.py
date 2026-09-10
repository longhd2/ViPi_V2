from __future__ import annotations

import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path


CLIENT = Path(__file__).resolve().parents[1]
PROJECT = CLIENT.parent
GENERATED = CLIENT / "generated"


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
    import os

    version = os.getenv("VIPI_CLIENT_BUILD_VERSION", "").strip()

    if not version:
        version = (
            CLIENT / "VERSION"
        ).read_text(
            encoding="utf-8"
        ).strip()

    commit = git_commit()
    now = datetime.now(timezone.utc)

    build_time = now.strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )

    build_id = (
        now.strftime("%Y%m%d-%H%M%S")
        + "-"
        + commit
    )

    GENERATED.mkdir(
        parents=True,
        exist_ok=True
    )

    data = {
        "product": "ViPi ESP32 Client",
        "version": version,
        "build_time": build_time,
        "build_id": build_id,
        "git_commit": commit,
    }

    (GENERATED / "build.json").write_text(
        json.dumps(
            data,
            ensure_ascii=False,
            indent=2
        ) + "\n",
        encoding="utf-8",
    )

    (GENERATED / "version.h").write_text(
        "#pragma once\n"
        f'#define VIPI_FW_VERSION "{version}"\n'
        f'#define VIPI_BUILD_ID "{build_id}"\n'
        f'#define VIPI_BUILD_TIME "{build_time}"\n',
        encoding="utf-8",
    )

    component_header = (
        CLIENT
        / "runtime_components"
        / "vipi_audio"
        / "version_generated.h"
    )

    component_header.write_text(
        "#pragma once\n"
        f'#define VIPI_FW_VERSION "{version}"\n'
        f'#define VIPI_BUILD_ID "{build_id}"\n'
        f'#define VIPI_BUILD_TIME "{build_time}"\n',
        encoding="utf-8",
    )

    print("VERSION   :", version)
    print("BUILD ID  :", build_id)
    print("BUILD TIME:", build_time)


if __name__ == "__main__":
    main()
