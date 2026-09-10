from __future__ import annotations

import os
import threading
from pathlib import Path

import yaml


BASE_DIR = Path(__file__).resolve().parent.parent
SECRETS_FILE = BASE_DIR / "config" / "secrets.yaml"


class SecretManager:
    def __init__(self, path=SECRETS_FILE):
        self.path = Path(path)
        self._lock = threading.RLock()

    def _load(self):
        if not self.path.exists():
            return {}

        try:
            return yaml.safe_load(
                self.path.read_text(encoding="utf-8")
            ) or {}
        except Exception:
            return {}

    def _save(self, data):
        self.path.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        tmp = self.path.with_suffix(".yaml.tmp")

        tmp.write_text(
            yaml.safe_dump(
                data,
                allow_unicode=True,
                sort_keys=False,
            ),
            encoding="utf-8",
        )

        os.chmod(tmp, 0o600)
        tmp.replace(self.path)
        os.chmod(self.path, 0o600)

    def get(self, section, key, default=""):
        with self._lock:
            data = self._load()
            section_data = data.get(section) or {}

            if not isinstance(section_data, dict):
                return default

            return section_data.get(key, default)

    def set(self, section, key, value):
        with self._lock:
            data = self._load()

            section_data = data.setdefault(
                section,
                {}
            )

            if not isinstance(section_data, dict):
                section_data = {}
                data[section] = section_data

            section_data[key] = value
            self._save(data)

    def configured(self, section, key):
        value = self.get(section, key, "")
        return bool(str(value).strip())


secret_manager = SecretManager()
