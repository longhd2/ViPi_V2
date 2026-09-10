import asyncio
import hashlib
import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SERVER_VERSION_FILE = ROOT / "VERSION"

CLIENT_MANIFEST_URL = (
    "https://raw.githubusercontent.com/"
    "longhd2/ViPi-ESP32-Firmware/main/latest.json"
)


def _version_tuple(value):
    try:
        return tuple(int(x) for x in str(value).strip().lstrip("v").split("."))
    except Exception:
        return (0,)


def server_version():
    try:
        return SERVER_VERSION_FILE.read_text(encoding="utf-8").strip()
    except Exception:
        return "unknown"


def _http_json(url, timeout=4):
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "ViPi-Server/2"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def _http_bytes(url, timeout=30):
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "ViPi-Server/2"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


async def latest_client():
    try:
        data = await asyncio.to_thread(
            _http_json,
            CLIENT_MANIFEST_URL,
        )
        return {
            "ok": True,
            "version": str(data.get("version", "")),
            "build_id": str(data.get("build_id", "")),
            "build_time": str(data.get("build_time", "")),
            "ota": data.get("ota", {}),
        }
    except Exception as e:
        return {
            "ok": False,
            "error": str(e),
        }


async def client_status(ip):
    if not ip:
        return {"ok": False, "error": "Thiếu IP"}

    url = f"http://{ip}/status"

    try:
        data = await asyncio.to_thread(_http_json, url, 2)
        data["ok"] = True
        return data
    except Exception as e:
        return {
            "ok": False,
            "error": str(e),
        }


async def update_client(ip):
    if not ip:
        return {"ok": False, "error": "Thiếu IP"}

    latest = await latest_client()

    if not latest.get("ok"):
        return latest

    ota = latest.get("ota") or {}
    url = ota.get("url")
    expected_sha = str(ota.get("sha256", "")).lower()

    if not url:
        return {
            "ok": False,
            "error": "Manifest không có OTA URL",
        }

    try:
        firmware = await asyncio.to_thread(
            _http_bytes,
            url,
            45,
        )
    except Exception as e:
        return {
            "ok": False,
            "error": f"Tải firmware lỗi: {e}",
        }

    actual_sha = hashlib.sha256(firmware).hexdigest()

    if expected_sha and actual_sha != expected_sha:
        return {
            "ok": False,
            "error": "SHA256 firmware không khớp",
        }

    def post_ota():
        req = urllib.request.Request(
            f"http://{ip}/ota",
            data=firmware,
            method="POST",
            headers={
                "Content-Type": "application/octet-stream",
                "X-Filename": "VIPI_V2_CLIENT_APP.bin",
            },
        )

        with urllib.request.urlopen(req, timeout=90) as r:
            return r.read().decode(
                "utf-8",
                errors="replace",
            )

    try:
        response = await asyncio.to_thread(post_ota)
    except Exception as e:
        return {
            "ok": False,
            "error": f"ESP32 OTA lỗi: {e}",
        }

    return {
        "ok": True,
        "version": latest.get("version"),
        "sha256": actual_sha,
        "size": len(firmware),
        "response": response,
    }


def compare_versions(installed, latest):
    return _version_tuple(latest) > _version_tuple(installed)
