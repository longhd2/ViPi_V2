from dataclasses import dataclass
from typing import Any

@dataclass
class SkillResult:
    handled: bool
    reply: str = ''
    data: dict[str,Any] | None = None

class Skill:
    name='base'; priority=100
    async def match(self,intent,ctx): return False
    async def handle(self,intent,ctx): raise NotImplementedError
