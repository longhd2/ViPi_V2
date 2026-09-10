import importlib
import inspect
import logging
from pathlib import Path

from .base import Skill

log = logging.getLogger("ViPiV2.SkillLoader")


class SkillLoader:
    """
    Tu dong quet skills/*.py.

    Mot skill chi can:
      - co 1 class ke thua Skill
      - khai bao MANIFEST

    Khong sua main.py khi them skill moi.
    Khong sua registry.
    Khong sua Understanding Engine.
    """

    SKIP = {
        "__init__",
        "base",
        "registry",
        "loader",
    }

    def __init__(self, registry):
        self.registry = registry

    def discover(self):
        root = Path(__file__).resolve().parent
        loaded = []

        for path in sorted(root.glob("*.py")):
            name = path.stem

            if name in self.SKIP or name.startswith("_"):
                continue

            module_name = f"skills.{name}"

            try:
                module = importlib.import_module(module_name)

            except Exception:
                log.exception(
                    "SKILL import failed %s",
                    module_name
                )
                continue

            manifest = getattr(module, "MANIFEST", None)

            if not isinstance(manifest, dict):
                log.debug(
                    "SKILL skip %s: no MANIFEST",
                    module_name
                )
                continue

            skill_class = None

            for _, cls in inspect.getmembers(
                module,
                inspect.isclass
            ):
                if cls is Skill:
                    continue

                if not issubclass(cls, Skill):
                    continue

                if cls.__module__ != module.__name__:
                    continue

                skill_class = cls
                break

            if skill_class is None:
                log.warning(
                    "SKILL %s co MANIFEST nhung khong co Skill class",
                    module_name
                )
                continue

            try:
                skill = skill_class()
                self.registry.register(
                    skill,
                    manifest
                )
                loaded.append(
                    manifest.get("name", name)
                )

            except Exception:
                log.exception(
                    "SKILL init failed %s",
                    module_name
                )

        log.info(
            "AUTO DISCOVERY skills=%d intents=%d",
            len(self.registry.skills),
            len(self.registry.intent_specs)
        )

        return loaded
