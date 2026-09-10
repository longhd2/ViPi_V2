import re

from datetime import datetime, timedelta
from zoneinfo import ZoneInfo

from .base import Skill, SkillResult
from scheduler.store import SchedulerStore


class TimerSkill(Skill):
    name = "timer"
    priority = 25
    TZ = ZoneInfo("Asia/Ho_Chi_Minh")

    def __init__(self):
        self.store = SchedulerStore("data/scheduler.json")

    def _text(self, intent):
        return (intent.slots.get("text") or "").lower().strip()

    def _seconds(self, text):
        units = {
            "giây": 1,
            "phút": 60,
            "tiếng": 3600,
            "giờ": 3600,
        }

        total = 0

        for value, unit in re.findall(
            r"(\d+)\s*(giây|phút|tiếng|giờ)",
            text,
        ):
            total += int(value) * units[unit]

        return total

    def _spoken_duration(self, seconds):
        if seconds < 60:
            return f"{seconds} giây"

        if seconds < 3600:
            m, s = divmod(seconds, 60)
            result = f"{m} phút"
            if s:
                result += f" {s} giây"
            return result

        h, remain = divmod(seconds, 3600)
        m = remain // 60

        result = f"{h} giờ"

        if m:
            result += f" {m} phút"

        return result

    async def handle(self, intent, ctx):
        text = self._text(intent)

        if intent.name == "timer.list":
            items = self.store.active(
                "timer",
                ctx.device_id,
            )

            if not items:
                return SkillResult(
                    True,
                    "Hiện không có hẹn giờ nào.",
                )

            parts = []

            for item in items[:5]:
                due = datetime.fromisoformat(item["due_at"])
                action = item.get("action") or {}

                if action:
                    desc = action.get("description") or action.get("intent")
                    parts.append(
                        f"{item['id']}, {desc}, lúc "
                        f"{due.hour} giờ {due.minute} phút"
                    )
                else:
                    parts.append(
                        f"{item['id']}, lúc "
                        f"{due.hour} giờ {due.minute} phút"
                    )

            return SkillResult(
                True,
                "Các hẹn giờ đang hoạt động: "
                + "; ".join(parts)
                + ".",
            )

        if intent.name == "timer.cancel":
            match = re.search(
                r"hủy hẹn giờ\s+([a-z0-9]+)",
                text,
            )

            if not match:
                return SkillResult(
                    True,
                    "Bạn hãy nói mã hẹn giờ cần hủy.",
                )

            item_id = match.group(1)

            if self.store.cancel(item_id, ctx.device_id):
                return SkillResult(
                    True,
                    f"Đã hủy hẹn giờ {item_id}.",
                )

            return SkillResult(
                True,
                "Tôi không tìm thấy hẹn giờ đó.",
            )

        seconds = int(intent.slots.get("seconds", 0) or 0)

        if seconds <= 0:
            seconds = self._seconds(text)

        if seconds <= 0:
            return SkillResult(
                True,
                "Bạn hãy nói thời lượng, ví dụ hẹn giờ 10 phút.",
            )

        due = datetime.now(self.TZ) + timedelta(seconds=seconds)

        item = self.store.add(
            "timer",
            due,
            "Hết giờ.",
            device_id=ctx.device_id,
        )

        return SkillResult(
            True,
            f"Đã đặt hẹn giờ {self._spoken_duration(seconds)}. "
            f"Mã {item['id']}.",
        )


MANIFEST = {
    "name": "timer",
    "intents": {
        "timer.cancel": {
            "phrases": [
                "hủy hẹn giờ",
            ],
            "patterns": [
                {
                    "regex": r"hủy hẹn giờ\s+[a-z0-9]+"
                },
            ],
            "slots": {
                "text": "copy_text",
            },
        },

        "timer.list": {
            "phrases": [
                "các hẹn giờ",
                "hẹn giờ đang có",
                "hẹn giờ đang hoạt động",
            ],
            "slots": {
                "text": "copy_text",
            },
        },

        "timer.set": {
            "phrases": [],
            "patterns": [
                {
                    "regex": (
                        r"(?:hẹn giờ|đặt hẹn giờ|đặt timer)"
                        r".*?\d+\s*(?:giây|phút|tiếng|giờ)"
                    )
                },
            ],
            "slots": {
                "seconds": "duration_seconds",
                "text": "copy_text",
            },
        },
    },
}
