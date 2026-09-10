import json
import os
import time
from pathlib import Path

STATE_FILE = Path(__file__).resolve().parents[1] / "data" / "clients.json"

_state = {}
_last_write = 0.0
WRITE_INTERVAL = 2.0


def touch(ctx):
    global _last_write

    now = time.time()

    meta = getattr(ctx, "metadata", {}) or {}
    device_id = getattr(ctx, "device_id", "unknown")

    _state[device_id] = {
        "device_id": device_id,
        "ip": meta.get("ip", ""),
        "room": getattr(ctx, "room", "default"),
        "volume": getattr(ctx, "volume", 70),
        "mode": "PTT" if meta.get("ptt") else "VOX",
        "firmware": meta.get("firmware", ""),
        "build_id": meta.get("build_id", ""),
        "last_seen": now,
    }

    if now - _last_write < WRITE_INTERVAL:
        return

    _last_write = now

    STATE_FILE.parent.mkdir(parents=True, exist_ok=True)

    tmp = STATE_FILE.with_suffix(".tmp")

    tmp.write_text(
        json.dumps(
            list(_state.values()),
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    os.replace(tmp, STATE_FILE)
