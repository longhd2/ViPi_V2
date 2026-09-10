from pathlib import Path
from typing import Any

import httpx
import yaml

from fastapi import APIRouter, Body, HTTPException, Query

router = APIRouter()

ROOT = Path(__file__).resolve().parents[1]
CONFIG_FILE = ROOT / "config" / "vipi.yaml"

DEFAULT_WEATHER = {
    "location_name": "Đà Nẵng",
    "latitude": 16.0544,
    "longitude": 108.2022,
    "timezone": "Asia/Ho_Chi_Minh",
    "forecast_days": 7,
    "rain_threshold": 50,
}


def _load() -> dict[str, Any]:
    if not CONFIG_FILE.exists():
        return {
            "server": {},
            "weather": dict(DEFAULT_WEATHER),
        }

    data = yaml.safe_load(
        CONFIG_FILE.read_text(encoding="utf-8")
    ) or {}

    data.setdefault("server", {})

    weather = dict(DEFAULT_WEATHER)
    weather.update(data.get("weather") or {})
    data["weather"] = weather

    return data


def _save(data: dict[str, Any]):
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)

    tmp = CONFIG_FILE.with_suffix(".yaml.tmp")

    tmp.write_text(
        yaml.safe_dump(
            data,
            allow_unicode=True,
            sort_keys=False,
        ),
        encoding="utf-8",
    )

    tmp.replace(CONFIG_FILE)


@router.get("/api/config")
async def get_config():
    return _load()


@router.put("/api/config/weather")
async def save_weather(payload: dict = Body(...)):
    try:
        location_name = str(
            payload.get("location_name", "")
        ).strip()

        latitude = float(payload["latitude"])
        longitude = float(payload["longitude"])

        timezone = str(
            payload.get(
                "timezone",
                "Asia/Ho_Chi_Minh"
            )
        ).strip()

        forecast_days = int(
            payload.get("forecast_days", 7)
        )

        rain_threshold = int(
            payload.get("rain_threshold", 50)
        )

    except Exception:
        raise HTTPException(
            400,
            "Thông số Weather không hợp lệ"
        )

    if not location_name:
        raise HTTPException(400, "Thiếu tên địa điểm")

    if not -90 <= latitude <= 90:
        raise HTTPException(400, "Latitude không hợp lệ")

    if not -180 <= longitude <= 180:
        raise HTTPException(400, "Longitude không hợp lệ")

    forecast_days = max(1, min(forecast_days, 16))
    rain_threshold = max(0, min(rain_threshold, 100))

    data = _load()

    data["weather"] = {
        "location_name": location_name,
        "latitude": latitude,
        "longitude": longitude,
        "timezone": timezone,
        "forecast_days": forecast_days,
        "rain_threshold": rain_threshold,
    }

    _save(data)

    return {
        "ok": True,
        "weather": data["weather"],
    }


@router.get("/api/geocode")
async def geocode(q: str = Query(min_length=2)):
    url = "https://geocoding-api.open-meteo.com/v1/search"

    params = {
        "name": q,
        "count": 8,
        "language": "vi",
        "format": "json",
    }

    try:
        async with httpx.AsyncClient(timeout=8) as client:
            r = await client.get(url, params=params)
            r.raise_for_status()
            raw = r.json()

    except Exception as e:
        raise HTTPException(
            502,
            f"Không thể tìm địa điểm: {e}"
        )

    results = []

    for x in raw.get("results", []):
        results.append({
            "name": x.get("name"),
            "admin1": x.get("admin1"),
            "admin2": x.get("admin2"),
            "country": x.get("country"),
            "latitude": x.get("latitude"),
            "longitude": x.get("longitude"),
            "timezone": x.get("timezone"),
        })

    return {
        "results": results,
    }


@router.post("/api/config/weather/test")
async def test_weather(payload: dict = Body(...)):
    try:
        lat = float(payload["latitude"])
        lon = float(payload["longitude"])
        tz = str(
            payload.get(
                "timezone",
                "Asia/Ho_Chi_Minh"
            )
        )

    except Exception:
        raise HTTPException(400, "Tọa độ không hợp lệ")

    params = {
        "latitude": lat,
        "longitude": lon,
        "timezone": tz,
        "current": "temperature_2m,weather_code",
        "daily": (
            "temperature_2m_max,"
            "temperature_2m_min,"
            "precipitation_probability_max"
        ),
        "forecast_days": 1,
    }

    try:
        async with httpx.AsyncClient(timeout=8) as client:
            r = await client.get(
                "https://api.open-meteo.com/v1/forecast",
                params=params
            )

            r.raise_for_status()
            weather = r.json()

    except Exception as e:
        raise HTTPException(
            502,
            f"Weather API lỗi: {e}"
        )

    current = weather.get("current") or {}
    daily = weather.get("daily") or {}

    def first(name):
        value = daily.get(name) or []
        return value[0] if value else None

    return {
        "ok": True,
        "location_name": payload.get(
            "location_name",
            ""
        ),
        "temperature": current.get(
            "temperature_2m"
        ),
        "weather_code": current.get(
            "weather_code"
        ),
        "min": first("temperature_2m_min"),
        "max": first("temperature_2m_max"),
        "rain": first(
            "precipitation_probability_max"
        ),
    }
