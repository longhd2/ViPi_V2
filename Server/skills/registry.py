import logging

log = logging.getLogger("ViPiV2.Registry")


class SkillRegistry:
    """
    Direct intent -> skill registry.

    Runtime dispatch KHONG loop qua tung skill.
    Moi intent duoc index 1 lan khi boot.
    """

    def __init__(self):
        self._skills = {}
        self._intent_to_skill = {}
        self._intent_specs = {}

    def register(self, skill, manifest):
        skill_name = manifest.get("name") or getattr(skill, "name", None)

        if not skill_name:
            raise ValueError("Skill thiếu name")

        if skill_name in self._skills:
            raise ValueError(f"Skill trùng: {skill_name}")

        self._skills[skill_name] = skill

        intents = manifest.get("intents") or {}

        for intent_name, spec in intents.items():
            if intent_name in self._intent_to_skill:
                raise ValueError(
                    f"Intent trùng: {intent_name}"
                )

            self._intent_to_skill[intent_name] = skill
            self._intent_specs[intent_name] = dict(spec or {})
            self._intent_specs[intent_name]["skill"] = skill_name

        log.info(
            "SKILL loaded %-16s intents=%d",
            skill_name,
            len(intents)
        )

    def get(self, intent_name):
        return self._intent_to_skill.get(intent_name)

    def get_spec(self, intent_name):
        return self._intent_specs.get(intent_name)

    @property
    def intent_specs(self):
        return dict(self._intent_specs)

    @property
    def skills(self):
        return dict(self._skills)

    async def dispatch(self, intent, ctx):
        """
        O(1): intent.name -> skill.
        KHONG goi skill.match().
        """

        skill = self._intent_to_skill.get(intent.name)

        if skill is None:
            log.warning(
                "NO SKILL for intent=%s",
                intent.name
            )
            return None

        try:
            return await skill.handle(intent, ctx)

        except Exception:
            log.exception(
                "SKILL failed skill=%s intent=%s",
                getattr(skill, "name", "?"),
                intent.name
            )
            return None
