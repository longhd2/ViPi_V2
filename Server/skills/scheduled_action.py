import re

from datetime import datetime, timedelta
from zoneinfo import ZoneInfo

from .base import Skill, SkillResult
from scheduler.store import SchedulerStore


class ScheduledActionSkill(Skill):
    name = "scheduled_action"
    priority = 20
    TZ = ZoneInfo("Asia/Ho_Chi_Minh")

    def __init__(self):
        self.store = SchedulerStore("data/scheduler.json")

    def _seconds(self, text):
        units = {
            "giây": 1,
            "phút": 60,
            "tiếng": 3600,
            "giờ": 3600,
        }

        match = re.search(
            r"(\d+)\s*(giây|phút|tiếng|giờ)",
            text,
        )

        if not match:
            return 0

        return int(match.group(1)) * units[match.group(2)]

    def _strip_delay(self, text):
        text = re.sub(
            r"^(?:hẹn giờ\s+)?"
            r"\d+\s*(?:giây|phút|tiếng|giờ)"
            r"\s*(?:sau|nữa)?\s*",
            "",
            text,
        )

        text = re.sub(
            r"^(?:sau|sau khi|trong)\s+"
            r"\d+\s*(?:giây|phút|tiếng|giờ)"
            r"\s*(?:thì|hãy)?\s*",
            "",
            text,
        )

        return text.strip(" ,.-")

    def _action(self, text):
        command = self._strip_delay(text)

        play = re.search(
            r"(?:mở|phát|bật|nghe)"
            r"(?:\s+bài hát|\s+bài|\s+nhạc)?\s+(.+)",
            command,
        )

        if play:
            query = play.group(1).strip()

            if query:
                return {
                    "intent": "media.play",
                    "slots": {
                        "query": query,
                    },
                    "description": f"phát {query}",
                }

        if re.search(
            r"\b(?:tắt|dừng|ngừng)\s+(?:nhạc|bài hát)\b",
            command,
        ):
            return {
                "intent": "media.stop",
                "slots": {},
                "description": "tắt nhạc",
            }

        return None

    async def handle(self, intent, ctx):
        text = (
            intent.slots.get("text")
            or ""
        ).lower().strip()

        seconds = self._seconds(text)
        action = self._action(text)

        if seconds <= 0:
            return SkillResult(
                True,
                "Tôi chưa xác định được thời gian thực hiện.",
            )

        if not action:
            return SkillResult(
                True,
                "Tôi chưa xác định được hành động cần thực hiện.",
            )

        due = datetime.now(self.TZ) + timedelta(seconds=seconds)

        item = self.store.add(
            "action",
            due,
            "",
            device_id=ctx.device_id,
            action=action,
        )

        return SkillResult(
            True,
            f"Đã hẹn {action['description']} sau "
            f"{seconds} giây. Mã {item['id']}.",
        )


MANIFEST = {
    "name": "scheduled_action",
    "intents": {
        "schedule.action": {
            "exclusive": True,
            "phrases": [],
            "patterns": [
                {
                    "regex": (
                        r"(?:hẹn giờ\s+)?"
                        r"\d+\s*(?:giây|phút|tiếng|giờ)"
                        r"\s*(?:sau|nữa)?\s*"
                        r"(?:mở|phát|bật|nghe)"
                        r"(?:\s+bài hát|\s+bài|\s+nhạc)?\s+.+"
                    )
                },
                {
                    "regex": (
                        r"(?:hẹn giờ\s+)?"
                        r"\d+\s*(?:giây|phút|tiếng|giờ)"
                        r"\s*(?:sau|nữa)?\s*"
                        r"(?:tắt|dừng|ngừng)\s+"
                        r"(?:nhạc|bài hát)"
                    )
                },
                {
                    "regex": (
                        r"(?:sau|trong)\s+"
                        r"\d+\s*(?:giây|phút|tiếng|giờ)"
                        r"\s*(?:thì|hãy)?\s*"
                        r"(?:mở|phát|bật|nghe)"
                        r"(?:\s+bài hát|\s+bài|\s+nhạc)?\s+.+"
                    )
                },
                {
                    "regex": (
                        r"(?:sau|trong)\s+"
                        r"\d+\s*(?:giây|phút|tiếng|giờ)"
                        r"\s*(?:thì|hãy)?\s*"
                        r"(?:tắt|dừng|ngừng)\s+"
                        r"(?:nhạc|bài hát)"
                    )
                },
            ],
            "slots": {
                "text": "copy_text",
            },
        },
    },
}
