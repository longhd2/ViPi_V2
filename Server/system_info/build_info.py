from __future__ import annotations

import json
import platform
import subprocess
from pathlib import Path


SERVER_DIR = Path(__file__).resolve().parent.parent
PROJECT_ROOT = SERVER_DIR.parent
VERSION_FILE = PROJECT_ROOT / "VERSION"
BUILD_FILE = SERVER_DIR / "system_info" / "build.json"


def _read_version():
    try:
        return VERSION_FILE.read_text(
            encoding="utf-8"
        ).strip()
    except Exception:
        return "unknown"


def _read_build():
    try:
        return json.loads(
            BUILD_FILE.read_text(encoding="utf-8")
        )
    except Exception:
        return {}


def _git(*args):
    try:
        return subprocess.check_output(
            ["git", *args],
            cwd=PROJECT_ROOT,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=2,
        ).strip()
    except Exception:
        return ""


def get_build_info():
    build = _read_build()
    commit = _git("rev-parse", "--short", "HEAD")

    dirty = None
    if commit:
        dirty = bool(_git("status", "--porcelain"))

    return {
        "product": "ViPi Server",
        "version": _read_version(),
        "build_time": build.get("build_time", "N/A"),
        "build_id": build.get("build_id", "N/A"),
        "python": platform.python_version(),
        "git_commit": commit or build.get("git_commit", "N/A"),
        "git_dirty": dirty,
        "no_llm": True,
    }
