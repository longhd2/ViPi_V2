import asyncio
import logging
import socket
import time

import numpy as np

from core.context import ClientContext
from core import client_status
from network.audio_codec import decode_ulaw
from audio.vad import VADSession
from network.protocol import send_audio


log = logging.getLogger("ViPiV2")


class Runtime:
    def __init__(self, router, stt, registry=None):
        self.router = router
        self.stt = stt
        self.registry = registry or getattr(router, "registry", None)

        self.devices = {}
        self.by_ip = {}
        # One independent streaming VAD state per ESP32.
        self.vad_sessions = {}
        self.last_packet = {}

        # Completed utterances only.
        self.queue = asyncio.Queue()

        # Serialize processing only inside the same ESP32.
        # Different devices may be handled concurrently.
        self.device_locks = {}

        self.udp_port = 3000
        self.tcp_port = 3001

    async def discovery(self):
        loop = asyncio.get_running_loop()

        sock = socket.socket(
            socket.AF_INET,
            socket.SOCK_DGRAM,
            socket.IPPROTO_UDP
        )

        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", 12345))

        group = socket.inet_aton("239.0.0.1")
        sock.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_ADD_MEMBERSHIP,
            group + socket.inet_aton("0.0.0.0")
        )

        sock.setblocking(False)

        log.info("MCAST 239.0.0.1:12345")

        while True:
            data, addr = await loop.sock_recvfrom(sock, 1024)

            try:
                parts = data.decode().split()
            except UnicodeDecodeError:
                continue

            if not parts:
                continue

            name = parts[0]
            ip = addr[0]

            ctx = self.devices.get(name)

            if ctx is None:
                ctx = ClientContext(
                    device_id=name,
                    room="default"
                )
                self.devices[name] = ctx

            ctx.metadata["ip"] = ip
            ctx.metadata["ptt"] = (
                "PTT" in [p.upper() for p in parts]
            )

            self.by_ip[ip] = ctx

            client_status.touch(ctx)

            # VAD state belongs to device_id, never IP.
            # Reconnect/IP change therefore keeps the logical device isolated.
            if name not in self.vad_sessions:
                self.vad_sessions[name] = VADSession()

            log.info(
                "DEVICE %s %s %s",
                name,
                ip,
                "PTT" if ctx.metadata["ptt"] else "VOX"
            )

    async def microphone(self):
        loop = asyncio.get_running_loop()

        sock = socket.socket(
            socket.AF_INET,
            socket.SOCK_DGRAM
        )

        sock.bind(("0.0.0.0", self.udp_port))
        sock.setblocking(False)

        log.info("UDP :3000")
        log.info(
            "VAD STREAMING 16kHz frame=512 "
            "threshold=0.50 neg=0.35 silence=650ms"
        )

        # Packet timeout is only a dead-stream safety mechanism.
        # Normal utterance endpointing is decided by Silero VAD.
        safety_timeout = 1.5

        while True:
            try:
                data, addr = await asyncio.wait_for(
                    loop.sock_recvfrom(sock, 2048),
                    timeout=0.2
                )

            except asyncio.TimeoutError:
                now = time.time()

                for name, last in list(self.last_packet.items()):
                    if now - last < safety_timeout:
                        continue

                    vad = self.vad_sessions.get(name)

                    if vad is not None and vad.recording:
                        audio = vad.flush()

                        if audio is not None:
                            ctx = self.devices.get(name)

                            if ctx:
                                log.info(
                                    "VAD SAFETY FLUSH %s %.2fs",
                                    name,
                                    len(audio) / 16000.0,
                                )

                                await self.queue.put(
                                    (ctx, audio)
                                )

                    self.last_packet.pop(name, None)

                continue

            ctx = self.by_ip.get(addr[0])

            if ctx is None:
                continue

            name = ctx.device_id
            self.last_packet[name] = time.time()

            vad = self.vad_sessions.get(name)

            if vad is None:
                vad = VADSession()
                self.vad_sessions[name] = vad

                log.info(
                    "VAD SESSION created device=%s",
                    name,
                )

            pcm = decode_ulaw(data)

            was_recording = vad.recording
            utterances = vad.feed(pcm)

            if vad.recording and not was_recording:
                log.info(
                    "VAD START device=%s prob=%.3f",
                    name,
                    vad.speech_prob,
                )

            for audio in utterances:
                log.info(
                    "VAD END device=%s duration=%.2fs",
                    name,
                    len(audio) / 16000.0,
                )

                await self.queue.put(
                    (ctx, audio)
                )

    async def processor(self, worker_id):
        while True:
            ctx, audio = await self.queue.get()

            try:
                device_id = ctx.device_id

                lock = self.device_locks.setdefault(
                    device_id,
                    asyncio.Lock(),
                )

                log.info(
                    "STT WORKER %d dequeued device=%s audio=%.2fs",
                    worker_id,
                    device_id,
                    len(audio) / 16000.0,
                )

                # Same device stays strictly ordered.
                # Different devices use independent locks.
                async with lock:
                    await self._process_utterance(
                        worker_id,
                        ctx,
                        audio,
                    )

            except Exception:
                log.exception(
                    "STT WORKER %d failed",
                    worker_id,
                )

            finally:
                self.queue.task_done()

    async def _process_utterance(
        self,
        worker_id,
        ctx,
        audio,
    ):
        from tts.edge import stream_to_device

        ip = ctx.metadata.get("ip")

        if not ip:
            return

        # VIPI_PERF_RUNTIME_V1
        _perf_total_t0 = time.perf_counter()
        _perf_stt_t0 = time.perf_counter()
        result = await self.stt.transcribe(
            audio.astype(np.int16).tobytes()
        )
        _perf_stt_ms = (time.perf_counter() - _perf_stt_t0) * 1000.0

        text = result.get("text", "").strip()

        if not text:
            return

        log.info(
            'STT WORKER %d ASR device=%s "%s"',
            worker_id,
            ctx.device_id,
            text,
        )

        _perf_router_t0 = time.perf_counter()
        routed = await self.router.handle_text(
            text,
            ctx,
        )
        _perf_router_ms = (time.perf_counter() - _perf_router_t0) * 1000.0

        intent = routed["intent"]

        log.info(
            "INTENT %s confidence=%.2f",
            intent.name,
            intent.confidence,
        )

        # ----------------------------------------------------
        # Individual short TTS replies.
        # Never concatenate multi-intent replies here.
        # ----------------------------------------------------

        replies = routed.get("replies")

        if replies is None:
            reply = routed.get("reply") or ""
            replies = [reply] if reply else []

        _perf_tts_total_ms = 0.0
        for index, reply in enumerate(
            replies,
            start=1
        ):
            reply = (reply or "").strip()

            if not reply:
                continue

            log.info(
                'TTS STEP %d/%d "%s"',
                index,
                len(replies),
                reply,
            )

            try:
                _perf_tts_t0 = time.perf_counter()
                await stream_to_device(
                    reply,
                    ip,
                    self.tcp_port,
                    voice="vi-VN-HoaiMyNeural",
                    volume=15,
                    fade=2,
                )
                _perf_tts_total_ms += (time.perf_counter() - _perf_tts_t0) * 1000.0

            except Exception:
                _perf_tts_total_ms += (time.perf_counter() - _perf_tts_t0) * 1000.0
                # One failed TTS must not prevent later replies
                # or deferred media from executing.
                log.exception(
                    "TTS STEP %d/%d failed",
                    index,
                    len(replies),
                )

        # ----------------------------------------------------
        # Log non-TTS command data from normal skills.
        # ----------------------------------------------------

        data = routed.get("data") or {}

        command = data.get("command")

        if command:
            log.info(
                "COMMAND %s %s",
                command,
                data,
            )

        # ----------------------------------------------------
        # media.play and future output-producing intents
        # execute only after all short TTS responses.
        # ----------------------------------------------------

        deferred = routed.get("deferred") or []

        if deferred:
            log.info(
                "DEFERRED EXECUTION count=%d",
                len(deferred),
            )

            try:
                await self.router.dispatch_deferred(
                    routed,
                    ctx,
                )

            except Exception:
                log.exception(
                    "DEFERRED EXECUTION failed"
                )

        _perf_total_ms = (time.perf_counter() - _perf_total_t0) * 1000.0
        log.info(
            'PERF REQUEST device=%s text="%s" STT=%.1fms ROUTER=%.1fms TTS_TOTAL=%.1fms TOTAL=%.1fms',
            ctx.device_id,
            text,
            _perf_stt_ms,
            _perf_router_ms,
            _perf_tts_total_ms,
            _perf_total_ms,
        )

    async def run(self):
        from media.manager import media_manager
        from scheduler.engine import SchedulerEngine

        scheduler = SchedulerEngine(
            self.devices,
            self.registry,
            store_path="data/scheduler.json",
            tcp_port=self.tcp_port,
        )

        await asyncio.gather(
            self.discovery(),
            self.microphone(),
            self.processor(1),
            self.processor(2),
            media_manager.proxy.run(),
            scheduler.run(),
        )
