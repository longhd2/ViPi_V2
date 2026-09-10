import asyncio
import logging

from understanding.engine import Intent
from .store import SchedulerStore


log = logging.getLogger("ViPiV2.Scheduler")


class SchedulerEngine:
    def __init__(
        self,
        devices,
        registry,
        store_path="data/scheduler.json",
        tcp_port=3001,
    ):
        self.devices = devices
        self.registry = registry
        self.store = SchedulerStore(store_path)
        self.tcp_port = tcp_port

    def _targets(self, item):
        device_id = item.get("device_id")

        if device_id:
            ctx = self.devices.get(device_id)
            return [ctx] if ctx else []

        return list(self.devices.values())

    async def _notify(self, item, ctx):
        from tts.edge import stream_to_device

        text = (item.get("message") or "").strip()

        if not text:
            if item.get("type") == "timer":
                text = "Hết giờ."
            else:
                text = "Đến giờ nhắc."

        ip = ctx.metadata.get("ip")

        if not ip:
            return

        log.info(
            'SCHEDULER TTS id=%s device=%s "%s"',
            item.get("id"),
            ctx.device_id,
            text,
        )

        await stream_to_device(
            text,
            ip,
            self.tcp_port,
            voice="vi-VN-HoaiMyNeural",
            volume=15,
            fade=2,
        )

    async def _dispatch_action(self, item, ctx):
        action = item.get("action") or {}

        intent_name = action.get("intent")

        if not intent_name:
            await self._notify(item, ctx)
            return

        slots = dict(action.get("slots") or {})

        intent = Intent(
            name=intent_name,
            slots=slots,
            confidence=1.0,
            skill="",
            order=1,
            source="scheduler",
            span=(0, 0),
        )

        log.info(
            "SCHEDULER ACTION id=%s device=%s intent=%s slots=%s",
            item.get("id"),
            ctx.device_id,
            intent_name,
            slots,
        )

        result = await self.registry.dispatch(
            intent,
            ctx,
        )

        if result is None:
            log.warning(
                "SCHEDULER ACTION failed id=%s intent=%s",
                item.get("id"),
                intent_name,
            )
            return

        reply = (
            getattr(result, "reply", "")
            or ""
        ).strip()

        if reply:
            copy = dict(item)
            copy["message"] = reply
            await self._notify(copy, ctx)

    async def _fire(self, item):
        targets = self._targets(item)

        if not targets:
            log.warning(
                "SCHEDULER no target id=%s device=%s",
                item.get("id"),
                item.get("device_id"),
            )
            return

        for ctx in targets:
            if ctx is None:
                continue

            try:
                if item.get("action"):
                    await self._dispatch_action(item, ctx)
                else:
                    await self._notify(item, ctx)

            except Exception:
                log.exception(
                    "SCHEDULER FIRE failed id=%s device=%s",
                    item.get("id"),
                    getattr(ctx, "device_id", "?"),
                )

    async def run(self):
        await asyncio.sleep(3)

        log.info("SCHEDULER started")

        while True:
            try:
                due = self.store.claim_due(
                    available_devices=set(self.devices.keys())
                )

                for item in due:
                    await self._fire(item)

            except Exception:
                log.exception("SCHEDULER loop failed")

            await asyncio.sleep(0.5)
