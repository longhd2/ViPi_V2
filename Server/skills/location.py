import re
import httpx
from .base import Skill, SkillResult


class LocationSkill(Skill):
    name = "location"
    priority = 78

    def _query(self, text):
        text = (text or "").lower()

        for prefix in (
            "tọa độ của",
            "toạ độ của",
            "tọa độ",
            "toạ độ",
            "vị trí của",
        ):
            if prefix in text:
                return text.split(prefix, 1)[1].strip(" ?.!")

        match = re.search(r"(.+?)\s+ở đâu", text)
        return match.group(1).strip() if match else ""

    async def handle(self, intent, ctx):
        query = self._query(intent.slots.get("text", ""))

        if not query:
            return SkillResult(True, "Bạn muốn tìm vị trí nào?")

        try:
            async with httpx.AsyncClient(timeout=6) as client:
                response = await client.get(
                    "https://geocoding-api.open-meteo.com/v1/search",
                    params={
                        "name": query,
                        "count": 1,
                        "language": "vi",
                        "format": "json",
                    },
                )
                response.raise_for_status()
                data = response.json()
        except Exception:
            return SkillResult(
                True,
                "Hiện tôi chưa tra được vị trí.",
            )

        results = data.get("results", [])

        if not results:
            return SkillResult(
                True,
                f"Tôi chưa tìm thấy {query}.",
            )

        item = results[0]

        place = ", ".join(
            x for x in (
                item.get("name", query),
                item.get("admin1", ""),
                item.get("country", ""),
            )
            if x
        )

        return SkillResult(
            True,
            f"{place}, tọa độ khoảng "
            f"{item.get('latitude')} vĩ độ và "
            f"{item.get('longitude')} kinh độ.",
        )


MANIFEST = {
    "name": "location",
    "intents": {
        "location.lookup": {
            "phrases": [
                "tọa độ",
                "toạ độ",
                "tọa độ của",
                "toạ độ của",
                "vị trí của",
                "ở đâu",
            ],
            "patterns": [
                {"regex": r".+\s+ở đâu$"},
            ],
            "copy_text": True,
        }
    },
}
