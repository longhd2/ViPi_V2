import re
import httpx
from .base import Skill, SkillResult


class WeatherSkill(Skill):
    name = "weather"
    priority = 85

    LATITUDE = 16.0544
    LONGITUDE = 108.2022
    LOCATION = "Đà Nẵng"
    TIMEZONE = "Asia/Ho_Chi_Minh"

    RAIN_CODES = {
        51, 53, 55,
        61, 63, 65,
        80, 81, 82,
        95, 96, 99,
    }

    def _weather_text(self, code):
        return {
            0: "trời quang",
            1: "trời khá quang",
            2: "có mây",
            3: "nhiều mây",
            45: "có sương mù",
            48: "có sương mù",
            51: "có mưa phùn nhẹ",
            53: "có mưa phùn",
            55: "có mưa phùn nhiều",
            61: "có mưa nhẹ",
            63: "có mưa",
            65: "có mưa lớn",
            71: "có tuyết nhẹ",
            73: "có tuyết",
            75: "có tuyết lớn",
            80: "có mưa rào nhẹ",
            81: "có mưa rào",
            82: "có mưa rào lớn",
            95: "có giông",
            96: "có giông và mưa đá",
            99: "có giông mạnh và mưa đá",
        }.get(code, "thời tiết thay đổi")

    def _day_offset(self, text):
        text = (text or "").lower()

        if re.search(r"\b(ngày mốt|ngày kia)\b", text):
            return 2

        if re.search(r"\bngày mai\b", text):
            return 1

        return 0

    def _day_label(self, offset):
        if offset == 1:
            return "Ngày mai"
        if offset == 2:
            return "Ngày mốt"
        return "Hôm nay"

    def _round_temp(self, value):
        if value is None:
            return None
        return int(round(float(value)))

    async def handle(self, intent, ctx):
        spoken_text = (
            intent.slots.get("text", "")
            if getattr(intent, "slots", None)
            else ""
        )

        day_offset = self._day_offset(spoken_text)

        params = {
            "latitude": self.LATITUDE,
            "longitude": self.LONGITUDE,
            "daily": (
                "weather_code,"
                "temperature_2m_max,"
                "temperature_2m_min,"
                "precipitation_probability_max"
            ),
            "timezone": self.TIMEZONE,
            "forecast_days": 3,
        }

        # Chỉ hôm nay mới cần current để biết thời tiết đang xảy ra.
        if day_offset == 0:
            params["current"] = (
                "temperature_2m,"
                "precipitation,"
                "weather_code"
            )

        try:
            async with httpx.AsyncClient(timeout=8) as client:
                response = await client.get(
                    "https://api.open-meteo.com/v1/forecast",
                    params=params,
                )
                response.raise_for_status()
                data = response.json()
        except Exception:
            return SkillResult(
                True,
                "Hiện tôi chưa lấy được dữ liệu thời tiết.",
            )

        daily = data.get("daily", {})

        codes = daily.get("weather_code", [])
        highs = daily.get("temperature_2m_max", [])
        lows = daily.get("temperature_2m_min", [])
        rains = daily.get("precipitation_probability_max", [])

        if (
            day_offset >= len(codes)
            or day_offset >= len(highs)
            or day_offset >= len(lows)
        ):
            return SkillResult(
                True,
                "Hiện tôi chưa lấy được dự báo cho ngày đó.",
            )

        daily_code = int(codes[day_offset])
        low = self._round_temp(lows[day_offset])
        high = self._round_temp(highs[day_offset])

        rain_probability = (
            rains[day_offset]
            if day_offset < len(rains)
            else None
        )

        if day_offset == 0:
            current = data.get("current", {})
            current_code = int(
                current.get("weather_code", daily_code)
            )
            current_precip = current.get("precipitation", 0) or 0

            raining_now = (
                current_code in self.RAIN_CODES
                or float(current_precip) > 0
            )

            answer = (
                f"{self.LOCATION} hiện "
                f"{self._weather_text(current_code)}. "
                f"Nhiệt độ hôm nay dao động từ "
                f"{low} đến {high} độ"
            )

            # Chỉ nói "dự báo có mưa" khi hiện tại chưa mưa.
            if (
                not raining_now
                and rain_probability is not None
                and float(rain_probability) >= 50
            ):
                answer += ", dự báo có mưa"

        else:
            label = self._day_label(day_offset)

            answer = (
                f"{label} {self.LOCATION} "
                f"{self._weather_text(daily_code)}. "
                f"Nhiệt độ dao động từ "
                f"{low} đến {high} độ"
            )

        return SkillResult(True, answer + ".")


MANIFEST = {
    "name": "weather",
    "intents": {
        "weather.current": {
            "phrases": [
                "thời tiết",
                "thời tiết hôm nay",
                "trời hôm nay",
                "hôm nay trời thế nào",
                "nhiệt độ hôm nay",
                "nhiệt độ",
                "trời mưa",
                "có mưa không",
                "nóng không",
                "lạnh không",

                "thời tiết ngày mai",
                "ngày mai thời tiết",
                "ngày mai trời thế nào",
                "trời ngày mai",
                "nhiệt độ ngày mai",

                "thời tiết ngày mốt",
                "ngày mốt thời tiết",
                "ngày mốt trời thế nào",
                "trời ngày mốt",
                "nhiệt độ ngày mốt",

                "thời tiết ngày kia",
                "ngày kia thời tiết",
                "ngày kia trời thế nào",
            ],
            "slots": {
                "text": "copy_text",
            },
        }
    },
}
