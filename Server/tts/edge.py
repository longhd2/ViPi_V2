import asyncio
import logging
import re

import edge_tts
import opuslib


log = logging.getLogger("ViPiV2")


VOICE = "vi-VN-HoaiMyNeural"


def normalize_vi_tts_text(text: str) -> str:
    """
    Normalize Vietnamese text before speech synthesis.

    Examples:
        23.7 -> 23 phẩy 7
        28.9 -> 28 phẩy 9
        33.7 -> 33 phẩy 7

    Only decimal dots between digits are converted.
    """
    if not text:
        return text

    return re.sub(
        r"(?<=\d)\.(?=\d)",
        " phẩy ",
        text,
    )


async def _open_esp32_stream(
    ip: str,
    port: int,
    mic_timeout: int = 60,
    volume: int = 15,
    fade: int = 2,
):
    _, writer = await asyncio.wait_for(
        asyncio.open_connection(ip, port),
        timeout=5,
    )

    header = bytes([
        0xAA,
        (mic_timeout >> 8) & 0xFF,
        mic_timeout & 0xFF,
        volume,
        fade,
        2,
    ])

    writer.write(header)
    await writer.drain()

    return writer


async def _stream_once(
    text: str,
    ip: str,
    port: int,
    voice: str,
    volume: int,
    fade: int,
):
    writer = None
    proc = None
    feeder_task = None

    first_edge = False
    first_pcm = False
    opus_packets = 0

    try:
        # -------------------------------------------------
        # ESP32 TCP
        # -------------------------------------------------
        writer = await _open_esp32_stream(
            ip,
            port,
            mic_timeout=60,
            volume=volume,
            fade=fade,
        )

        # -------------------------------------------------
        # FFmpeg: MP3 stream -> PCM S16LE mono 16 kHz
        # -------------------------------------------------
        proc = await asyncio.create_subprocess_exec(
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-f",
            "mp3",
            "-i",
            "pipe:0",
            "-f",
            "s16le",
            "-acodec",
            "pcm_s16le",
            "-ac",
            "1",
            "-ar",
            "16000",
            "pipe:1",
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )

        communicate = edge_tts.Communicate(
            text,
            voice,
        )

        # -------------------------------------------------
        # EDGE TTS STREAM
        # Không save MP3 xuống disk
        # -------------------------------------------------
        async def feed_edge_audio():
            nonlocal first_edge

            try:
                async for message in communicate.stream():

                    if message.get("type") != "audio":
                        continue

                    chunk = message.get("data", b"")

                    if not chunk:
                        continue

                    if not first_edge:
                        first_edge = True
                        log.info(
                            "TTS STREAM first Edge audio chunk=%d bytes",
                            len(chunk),
                        )

                    proc.stdin.write(chunk)
                    await proc.stdin.drain()

            finally:
                if proc and proc.stdin:
                    try:
                        proc.stdin.close()
                    except Exception:
                        pass

        feeder_task = asyncio.create_task(
            feed_edge_audio()
        )

        # -------------------------------------------------
        # OPUS
        # -------------------------------------------------
        encoder = opuslib.Encoder(
            16000,
            1,
            opuslib.APPLICATION_VOIP,
        )

        frame_size = 320          # 20 ms @ 16 kHz
        frame_bytes = frame_size * 2
        pcm_buffer = bytearray()

        while True:
            chunk = await proc.stdout.read(4096)

            if not chunk:
                break

            if not first_pcm:
                first_pcm = True
                log.info(
                    "TTS STREAM first PCM chunk=%d bytes",
                    len(chunk),
                )

            pcm_buffer.extend(chunk)

            while len(pcm_buffer) >= frame_bytes:

                frame = bytes(
                    pcm_buffer[:frame_bytes]
                )

                del pcm_buffer[:frame_bytes]

                packet = encoder.encode(
                    frame,
                    frame_size,
                )

                writer.write(
                    len(packet).to_bytes(2, "big")
                )
                writer.write(packet)

                opus_packets += 1

                if opus_packets % 10 == 0:
                    await writer.drain()

        # -------------------------------------------------
        # Chờ Edge TTS kết thúc
        # -------------------------------------------------
        await feeder_task

        # -------------------------------------------------
        # Frame cuối
        # -------------------------------------------------
        if pcm_buffer:

            frame = bytes(pcm_buffer)

            if len(frame) < frame_bytes:
                frame += b"\x00" * (
                    frame_bytes - len(frame)
                )

            packet = encoder.encode(
                frame,
                frame_size,
            )

            writer.write(
                len(packet).to_bytes(2, "big")
            )
            writer.write(packet)

            opus_packets += 1

        await proc.wait()

        if proc.returncode != 0:

            error = (
                await proc.stderr.read()
            ).decode(
                "utf-8",
                errors="ignore",
            ).strip()

            raise RuntimeError(
                f"ffmpeg TTS failed: {error}"
            )

        # Edge không trả audio
        if not first_edge:
            raise RuntimeError(
                "Edge TTS returned no audio"
            )

        if not first_pcm:
            raise RuntimeError(
                "TTS decoder returned no PCM"
            )

        # -------------------------------------------------
        # EOS
        # -------------------------------------------------
        writer.write(b"\x00\x00")
        await writer.drain()

        log.info(
            "TTS STREAM complete packets=%d",
            opus_packets,
        )

    finally:

        if feeder_task:
            if not feeder_task.done():
                feeder_task.cancel()

            try:
                await feeder_task
            except (
                asyncio.CancelledError,
                Exception,
            ):
                pass

        if proc and proc.returncode is None:
            try:
                proc.kill()
            except Exception:
                pass

            try:
                await proc.wait()
            except Exception:
                pass

        if writer:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass


async def stream_to_device(
    text: str,
    ip: str,
    port: int = 3001,
    voice: str = VOICE,
    volume: int = 15,
    fade: int = 2,
):
    text = normalize_vi_tts_text(text)

    """
    V2 TTS streaming:

        Edge TTS
           ↓
        MP3 chunks
           ↓
        FFmpeg stdin
           ↓
        PCM S16LE / mono / 16k
           ↓
        Opus 20 ms
           ↓
        TCP :3001
           ↓
        ESP32

    Không tạo file MP3 trung gian.
    """

    last_error = None

    for attempt in range(1, 4):

        log.info(
            'TTS STREAM attempt %d/3 "%s"',
            attempt,
            text,
        )

        try:

            await _stream_once(
                text,
                ip,
                port,
                voice,
                volume,
                fade,
            )

            return

        except Exception as e:

            last_error = e

            log.warning(
                "TTS STREAM attempt %d/3 failed: %s",
                attempt,
                e,
            )

            if attempt < 3:
                await asyncio.sleep(1.0 * attempt)

    raise RuntimeError(
        f"Edge TTS failed after 3 streaming attempts: "
        f"{last_error}"
    )
