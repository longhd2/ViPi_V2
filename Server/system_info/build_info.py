from __future__ import annotations

import os
import platform
import subprocess
from datetime import datetime
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent.parent

VERSION = os.getenv(
    "VIPI_SERVER_VERSION",
    "2.0.0",
)

BUILD_TIME = os.getenv(
    "VIPI_BUILD_TIME",
    datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
)

BUILD_ID = os.getenv(
    "VIPI_BUILD_ID",
    datetime.now().strftime("%Y%m%d-%H%M%S"),
)


def _git(*args):
    try:
        return subprocess.check_output(
            ["git", *args],
            cwd=BASE_DIR,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=2,
        ).strip()
    except Exception:
        return ""


def get_build_info():
    commit = _git(
        "rev-parse",
        "--short",
        "HEAD",
    )

    dirty = False

    if commit:
        dirty = bool(
            _git("status", "--porcelain")
        )

    return {
        "product": "ViPi Server",
        "version": VERSION,
        "build_time": BUILD_TIME,
        "build_id": BUILD_ID,
        "python": platform.python_version(),
        "git_commit": commit or "N/A",
        "git_dirty": dirty if commit else None,
        "no_llm": True,
    }
