import asyncio
import logging

from core.router import Router
from understanding.engine import UnderstandingEngine

from skills.registry import SkillRegistry
from skills.loader import SkillLoader

from stt.client import STTClient
from network.runtime import Runtime


def build_app():
    registry = SkillRegistry()

    loader = SkillLoader(
        registry
    )

    loader.discover()

    engine = UnderstandingEngine(
        registry.intent_specs
    )

    return Router(
        engine,
        registry
    )


async def main():
    logging.basicConfig(
        level=logging.INFO,
        format=(
            "%(asctime)s  "
            "%(levelname)s  "
            "%(message)s"
        ),
        datefmt="%H:%M:%S"
    )

    logging.info(
        "==================================="
    )
    logging.info(
        " ViPi Server V2"
    )
    logging.info(
        " NO LLM"
    )
    logging.info(
        " AUTO SKILL DISCOVERY"
    )
    logging.info(
        " MULTI INTENT ENGINE"
    )
    logging.info(
        " DIRECT INTENT ROUTING O(1)"
    )
    logging.info(
        " STT http://127.0.0.1:8100"
    )
    logging.info(
        " UDP :3000"
    )
    logging.info(
        " TCP ESP32 :3001"
    )
    logging.info(
        " MCAST 239.0.0.1:12345"
    )
    logging.info(
        "==================================="
    )

    router = build_app()
    stt = STTClient()

    runtime = Runtime(
        router,
        stt,
        registry=router.registry,
    )

    await runtime.run()


if __name__ == "__main__":
    asyncio.run(main())
