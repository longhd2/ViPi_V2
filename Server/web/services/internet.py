import asyncio
import time

async def internet_status():
    started = time.monotonic()
    try:
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection("1.1.1.1", 443), timeout=2.0
        )
        writer.close()
        await writer.wait_closed()
        return {
            "online": True,
            "latency_ms": round((time.monotonic() - started) * 1000)
        }
    except Exception:
        return {"online": False, "latency_ms": None}
