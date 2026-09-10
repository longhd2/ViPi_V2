#!/usr/bin/env python3
import asyncio
import sys
from pathlib import Path

SERVER_DIR = Path(__file__).resolve().parent.parent
if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from skills.registry import SkillRegistry
from skills.loader import SkillLoader
from understanding.engine import UnderstandingEngine
from core.router import Router
from core.context import ClientContext


async def main():
    text = " ".join(sys.argv[1:]).strip()

    if not text:
        print("LOI: Chua co cau test")
        return

    registry = SkillRegistry()
    loader = SkillLoader(registry)
    loaded = loader.discover()

    engine = UnderstandingEngine(registry.intent_specs)
    router = Router(engine, registry)

    ctx = ClientContext(
        device_id="vipi-test-console",
        room="default",
        volume=70,
        online=True,
        metadata={
            "ip": "127.0.0.1",
            "source": "test-console",
        },
    )

    print()
    print("========================================")
    print(" VIPI TEXT TEST")
    print("========================================")
    print("TEXT    :", text)

    plan = engine.resolve(text, ctx)

    print(
        "PLAN    :",
        [x.name for x in plan.intents]
    )

    for x in plan.intents:
        print(
            "INTENT  :",
            x.name,
            "| confidence =",
            round(x.confidence, 3),
            "| slots =",
            x.slots,
        )

    routed = await router.handle_text(text, ctx)

    print("REPLIES :", routed.get("replies") or [])
    print("REPLY   :", routed.get("reply") or "")
    print("DATA    :", routed.get("data") or {})
    print("DEFERRED:", [
        getattr(x, "name", str(x))
        for x in (routed.get("deferred") or [])
    ])
    print("========================================")


asyncio.run(main())
