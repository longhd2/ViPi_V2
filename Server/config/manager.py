from __future__ import annotations

import copy
import threading
from pathlib import Path

import yaml


BASE_DIR = Path(__file__).resolve().parent.parent
CONFIG_FILE = BASE_DIR / "config" / "vipi.yaml"

DEFAULT_CONFIG = {
    "server": {
        "name": "ViPi Server",
        "admin_host": "0.0.0.0",
        "admin_port": 8090,
    },
    "weather": {
        "location_name": "Đà Nẵng",
        "latitude": 16.0544,
        "longitude": 108.2022,
        "timezone": "Asia/Ho_Chi_Minh",
        "forecast_days": 7,
        "rain_threshold": 50,
    },
    "home_assistant": {
        "enabled": False,
        "internal_url": "",
        "external_url": "",
        "prefer_internal": True,
        "timeout": 5,
    },
}


class ConfigManager:
    def __init__(self, path=CONFIG_FILE):
        self.path = Path(path)
        self._lock = threading.RLock()
        self._config = {}
        self.load()

    def _merge(self, base, override):
        result = copy.deepcopy(base)

        for key, value in (override or {}).items():
            if (
                isinstance(value, dict)
                and isinstance(result.get(key), dict)
            ):
                result[key] = self._merge(
                    result[key],
                    value,
                )
            else:
                result[key] = value

        return result

    def load(self):
        with self._lock:
            raw = {}

            if self.path.exists():
                try:
                    raw = yaml.safe_load(
                        self.path.read_text(
                            encoding="utf-8"
                        )
                    ) or {}
                except Exception:
                    raw = {}

            self._config = self._merge(
                DEFAULT_CONFIG,
                raw,
            )

            if not self.path.exists():
                self.save()

            return copy.deepcopy(self._config)

    def save(self):
        with self._lock:
            self.path.parent.mkdir(
                parents=True,
                exist_ok=True,
            )

            tmp = self.path.with_suffix(".yaml.tmp")

            tmp.write_text(
                yaml.safe_dump(
                    self._config,
                    allow_unicode=True,
                    sort_keys=False,
                ),
                encoding="utf-8",
            )

            tmp.replace(self.path)

    def get(self, section, key=None, default=None):
        with self._lock:
            value = self._config.get(
                section,
                default,
            )

            if key is None:
                return copy.deepcopy(value)

            if not isinstance(value, dict):
                return default

            return copy.deepcopy(
                value.get(key, default)
            )

    def update_section(self, section, values):
        with self._lock:
            current = self._config.setdefault(
                section,
                {},
            )

            if not isinstance(current, dict):
                current = {}
                self._config[section] = current

            current.update(values)
            self.save()

            return copy.deepcopy(current)

    def all(self):
        with self._lock:
            return copy.deepcopy(self._config)


config_manager = ConfigManager()
