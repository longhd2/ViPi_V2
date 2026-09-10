import httpx
import numpy as np

from network.audio_codec import pcm_to_wav


class STTClient:
    def __init__(self, url="http://127.0.0.1:8100"):
        self.url = url.rstrip("/")

    async def transcribe(self, pcm):
        wav = pcm_to_wav(
            np.frombuffer(pcm, dtype=np.int16),
            16000
        )

        async with httpx.AsyncClient(timeout=30) as client:
            response = await client.post(
                self.url + "/transcribe",
                files={
                    "audio": (
                        "audio.wav",
                        wav,
                        "audio/wav"
                    )
                }
            )

        response.raise_for_status()
        return response.json()
