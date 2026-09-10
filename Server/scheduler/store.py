import json
import os
import uuid

from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo


class SchedulerStore:
    TZ = ZoneInfo("Asia/Ho_Chi_Minh")

    def __init__(self, path="data/scheduler.json"):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)

    def _load(self):
        if not self.path.exists():
            return []

        try:
            with open(self.path, encoding="utf-8") as f:
                data = json.load(f)

            return data if isinstance(data, list) else []
        except Exception:
            return []

    def _save(self, items):
        tmp = str(self.path) + ".tmp"

        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(
                items,
                f,
                ensure_ascii=False,
                indent=2,
            )

        os.replace(tmp, self.path)

    def add(
        self,
        item_type,
        due_at,
        message="",
        device_id=None,
        action=None,
    ):
        items = self._load()

        item = {
            "id": uuid.uuid4().hex[:6],
            "type": item_type,
            "due_at": due_at.isoformat(),
            "message": message,
            "device_id": device_id,
            "action": action,
            "created_at": datetime.now(self.TZ).isoformat(),
            "status": "active",
        }

        items.append(item)
        self._save(items)
        return item

    def active(self, item_type=None, device_id=None):
        result = []

        for item in self._load():
            if item.get("status") != "active":
                continue

            if item_type and item.get("type") != item_type:
                continue

            owner = item.get("device_id")

            if device_id and owner and owner != device_id:
                continue

            result.append(item)

        return sorted(
            result,
            key=lambda x: x.get("due_at", ""),
        )

    def cancel(self, item_id, device_id=None):
        items = self._load()

        for item in items:
            if item.get("id") != item_id:
                continue

            if item.get("status") != "active":
                continue

            owner = item.get("device_id")

            if device_id and owner and owner != device_id:
                continue

            item["status"] = "cancelled"
            item["cancelled_at"] = datetime.now(self.TZ).isoformat()

            self._save(items)
            return True

        return False

    def claim_due(self, now=None, available_devices=None):
        if now is None:
            now = datetime.now(self.TZ)

        available = (
            set(available_devices)
            if available_devices is not None
            else None
        )

        items = self._load()
        result = []
        changed = False

        for item in items:
            if item.get("status") != "active":
                continue

            try:
                due = datetime.fromisoformat(item["due_at"])

                if due.tzinfo is None:
                    due = due.replace(tzinfo=self.TZ)

            except Exception:
                continue

            if due > now:
                continue

            if available is not None:
                owner = item.get("device_id")

                if owner:
                    if owner not in available:
                        continue
                elif not available:
                    continue

            item["status"] = "fired"
            item["fired_at"] = now.isoformat()

            result.append(dict(item))
            changed = True

        if changed:
            self._save(items)

        return result
