import re

from datetime import datetime, timedelta
from zoneinfo import ZoneInfo

from dateparser.search import search_dates

from .base import Skill, SkillResult
from scheduler.store import SchedulerStore


class ReminderSkill(Skill):
    name = "reminder"
    priority = 24

    TZ = ZoneInfo(
        "Asia/Ho_Chi_Minh"
    )

    def __init__(self):
        self.store = SchedulerStore(
            "data/scheduler.json"
        )

    def _text(self, intent):
        return (
            intent.slots.get("text")
            or ""
        ).lower().strip()

    def _relative_due(
        self,
        text,
    ):
        match = re.search(
            r"sau\s+(\d+)\s*"
            r"(giây|phút|giờ|ngày)",
            text,
        )

        if not match:
            return None

        value = int(
            match.group(1)
        )
        unit = match.group(2)

        kwargs = {}

        if unit == "giây":
            kwargs["seconds"] = value
        elif unit == "phút":
            kwargs["minutes"] = value
        elif unit == "giờ":
            kwargs["hours"] = value
        else:
            kwargs["days"] = value

        return (
            datetime.now(self.TZ)
            + timedelta(**kwargs)
        )

    def _absolute_due(
        self,
        text,
    ):
        found = search_dates(
            text,
            languages=["vi"],
            settings={
                "PREFER_DATES_FROM": "future",
                "TIMEZONE": "Asia/Ho_Chi_Minh",
                "RETURN_AS_TIMEZONE_AWARE": True,
            },
        )

        if not found:
            return None

        return found[-1][1]

    def _message(
        self,
        text,
    ):
        text = re.sub(
            r"^.*?nhắc tôi\s+",
            "",
            text,
        )

        text = re.sub(
            r"sau\s+\d+\s*"
            r"(giây|phút|giờ|ngày)",
            "",
            text,
        )

        text = text.strip(
            " ,.-"
        )

        if not text:
            return "việc đã đặt"

        return text

    async def handle(
        self,
        intent,
        ctx,
    ):
        text = self._text(
            intent
        )

        if intent.name == "reminder.list":
            items = self.store.active(
                "reminder",
                ctx.device_id,
            )

            if not items:
                return SkillResult(
                    True,
                    "Hiện không có lời nhắc nào.",
                )

            parts = []

            for item in items[:5]:
                due = datetime.fromisoformat(
                    item["due_at"]
                )

                parts.append(
                    f"{item['id']}: "
                    f"{item['message']}, "
                    f"lúc {due.hour} giờ "
                    f"{due.minute} phút"
                )

            return SkillResult(
                True,
                "Các lời nhắc: "
                + "; ".join(parts)
                + ".",
            )

        if intent.name == "reminder.cancel":
            match = re.search(
                r"hủy lời nhắc\s+([a-z0-9]+)",
                text,
            )

            if not match:
                return SkillResult(
                    True,
                    "Bạn hãy nói mã lời nhắc cần hủy.",
                )

            item_id = match.group(1)

            if self.store.cancel(
                item_id,
                ctx.device_id,
            ):
                return SkillResult(
                    True,
                    f"Đã hủy lời nhắc {item_id}.",
                )

            return SkillResult(
                True,
                "Tôi không tìm thấy lời nhắc đó.",
            )

        due = self._relative_due(
            text
        )

        if due is None:
            due = self._absolute_due(
                text
            )

        if due is None:
            return SkillResult(
                True,
                "Tôi chưa xác định được thời gian cần nhắc.",
            )

        message = self._message(
            text
        )

        item = self.store.add(
            "reminder",
            due,
            message,
            device_id=ctx.device_id,
        )

        return SkillResult(
            True,
            f"Đã tạo lời nhắc {message}, "
            f"mã {item['id']}.",
        )


MANIFEST = {
    "name": "reminder",
    "intents": {
        "reminder.cancel": {
            "phrases": [
                "hủy lời nhắc",
            ],
            "patterns": [
                {"regex": r"hủy lời nhắc\s+[a-z0-9]+"},
            ],
            "slots": {
                "text": "copy_text",
            },
        },

        "reminder.list": {
            "phrases": [
                "các lời nhắc",
                "lời nhắc đang có",
            ],
            "slots": {
                "text": "copy_text",
            },
        },

        "reminder.set": {
            "phrases": [
                "nhắc tôi",
                "nhắc việc",
                "tạo lời nhắc",
            ],
            "patterns": [
                {"regex": r"nhắc tôi.*sau\s+\d+\s*(?:giây|phút|giờ|ngày)"},
            ],
            "slots": {
                "text": "copy_text",
            },
        },
    },
}
