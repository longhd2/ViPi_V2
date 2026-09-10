import asyncio


async def send_audio(ip, port, payload, mic_timeout=60, volume=15, fade=2):
    try:
        _, writer = await asyncio.wait_for(
            asyncio.open_connection(ip, port),
            timeout=5
        )

        header = bytes([
            0xAA,
            (mic_timeout >> 8) & 0xFF,
            mic_timeout & 0xFF,
            volume,
            fade,
            2
        ])

        writer.write(header)
        if payload:
            writer.write(payload)

        await writer.drain()
        writer.close()
        await writer.wait_closed()

    except (asyncio.TimeoutError, ConnectionError, OSError):
        pass
