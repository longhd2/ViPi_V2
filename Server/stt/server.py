"""
ViPi Google Speech Recognition ASR Server

Run:
    python -m pipeline.services.asr_server

API:
    GET  /health
    POST /transcribe

Google Speech Recognition:
    - Nhận tiếng Việt: vi-VN
    - Cần Internet
    - Không cần chạy model ASR trên CPU
"""

import logging
import os
import tempfile
import time
import traceback
import wave

import speech_recognition as sr
from fastapi import FastAPI, File, Request, UploadFile
from fastapi.responses import JSONResponse


# ============================================================
# CẤU HÌNH
# ============================================================

LANGUAGE = "vi-VN"

recognizer = sr.Recognizer()


# ============================================================
# LOG
# ============================================================

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)

logger = logging.getLogger("vipi-asr")


# ============================================================
# FASTAPI
# ============================================================

app = FastAPI(
    title="ViPi ASR",
    description="Vietnamese ASR using Google Speech Recognition",
)


# ============================================================
# ERROR HANDLER
# ============================================================

@app.exception_handler(Exception)
async def unhandled_exception_handler(
    request: Request,
    exc: Exception,
):
    logger.error(
        "Unhandled exception on %s %s\n%s",
        request.method,
        request.url.path,
        traceback.format_exc(),
    )

    return JSONResponse(
        status_code=500,
        content={"error": str(exc)},
    )


# ============================================================
# STARTUP
# ============================================================

@app.on_event("startup")
async def startup():

    logger.info(
        "ViPi Google Speech Recognition ready | language=%s",
        LANGUAGE,
    )


# ============================================================
# HEALTH CHECK
# ============================================================

@app.get("/health")
async def health():

    return {
        "status": "ok",
        "engine": "google",
        "language": LANGUAGE,
        "online": True,
    }


# ============================================================
# AUDIO DURATION
# ============================================================

def get_wav_duration(path):

    try:

        with wave.open(path, "rb") as wav:

            frames = wav.getnframes()
            rate = wav.getframerate()

            if rate <= 0:
                return 0.0

            return frames / float(rate)

    except Exception:

        return 0.0


# ============================================================
# TRANSCRIBE
# ============================================================

@app.post("/transcribe")
async def transcribe(
    audio: UploadFile = File(...)
):

    raw = await audio.read()

    ext = os.path.splitext(
        audio.filename or "audio.wav"
    )[1] or ".wav"

    with tempfile.NamedTemporaryFile(
        suffix=ext,
        delete=False,
    ) as f:

        f.write(raw)
        tmp_path = f.name

    tic = time.time()

    text = ""
    duration_s = 0.0

    try:

        duration_s = get_wav_duration(tmp_path)

        # Đọc chính file WAV mà pipeline gửi tới ASR.
        with sr.AudioFile(tmp_path) as source:

            audio_data = recognizer.record(source)

        try:

            # Google Web Speech Recognition.
            # Không cần API key cho phương thức recognize_google().
            text = recognizer.recognize_google(
                audio_data,
                language=LANGUAGE,
            )

        except sr.UnknownValueError:

            logger.warning(
                "Google ASR could not understand audio"
            )

            text = ""

        except sr.RequestError as exc:

            logger.error(
                "Google ASR request failed: %s",
                exc,
            )

            raise

        elapsed = time.time() - tic

        logger.info(
            'GOOGLE ASR: "%s" | audio %.2fs | processing %.2fs',
            text,
            duration_s,
            elapsed,
        )

    except Exception:

        logger.error(
            "Transcription failed for %s\n%s",
            audio.filename,
            traceback.format_exc(),
        )

        raise

    finally:

        try:
            os.unlink(tmp_path)
        except OSError:
            pass


    # Giữ nguyên JSON tương thích Onju-v2.
    return {
        "text": text.strip(),
        "duration_s": round(duration_s, 2),
        "transcribe_time_s": round(
            time.time() - tic,
            3,
        ),
    }


# ============================================================
# MAIN
# ============================================================

if __name__ == "__main__":

    import argparse
    import uvicorn

    parser = argparse.ArgumentParser(
        description="ViPi Google Speech Recognition ASR Server"
    )

    parser.add_argument(
        "--port",
        type=int,
        default=8100,
    )

    parser.add_argument(
        "--host",
        default="127.0.0.1",
    )

    args = parser.parse_args()

    uvicorn.run(
        app,
        host=args.host,
        port=args.port,
    )