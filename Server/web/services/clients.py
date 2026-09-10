import json
import time
from pathlib import Path

STATE = Path(__file__).resolve().parents[2] / "data" / "clients.json"

def get_clients():
    try:
        data = json.loads(STATE.read_text(encoding="utf-8"))
    except Exception:
        return []

    now = time.time()
    result = []

    for item in data if isinstance(data, list) else []:
        last_seen = float(item.get("last_seen", 0) or 0)
        age = max(0, int(now - last_seen)) if last_seen else None
        row = dict(item)
        row["age"] = age
        row["online"] = bool(last_seen and age <= 30)
        result.append(row)

    return result
