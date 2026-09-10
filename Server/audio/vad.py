from __future__ import annotations

from collections import deque
import logging

import numpy as np
import onnxruntime

log = logging.getLogger("ViPiV2.VAD")

SAMPLE_RATE = 16000
FRAME_SIZE = 512
CONTEXT_SIZE = 64


def _load_model():
    import silero_vad
    from pathlib import Path

    model_path = Path(silero_vad.__file__).parent / "data" / "silero_vad.onnx"

    opts = onnxruntime.SessionOptions()
    opts.inter_op_num_threads = 1
    opts.intra_op_num_threads = 1

    return onnxruntime.InferenceSession(
        str(model_path),
        sess_options=opts,
    )


class VADSession:
    """
    Streaming Silero VAD state for ONE ViPi device.

    One VADSession must never receive PCM from another device.
    """

    def __init__(
        self,
        threshold: float = 0.50,
        neg_threshold: float = 0.35,
        silence_ms: int = 650,
        pre_buffer_ms: int = 320,
        min_utterance_ms: int = 300,
        max_utterance_s: float = 20.0,
    ):
        self.threshold = threshold
        self.neg_threshold = neg_threshold

        self.silence_frames = max(
            1,
            int((silence_ms / 1000.0) * SAMPLE_RATE / FRAME_SIZE),
        )

        self.pre_frames = max(
            1,
            int((pre_buffer_ms / 1000.0) * SAMPLE_RATE / FRAME_SIZE),
        )

        self.min_samples = int(
            SAMPLE_RATE * min_utterance_ms / 1000.0
        )

        self.max_samples = int(
            SAMPLE_RATE * max_utterance_s
        )

        self.session = _load_model()

        self.pre_buffer = deque(maxlen=self.pre_frames)
        self.buffer = []

        self.pending = np.empty(0, dtype=np.int16)

        self.recording = False
        self.silence_count = 0
        self.speech_prob = 0.0

        self._state = np.zeros(
            (2, 1, 128),
            dtype=np.float32,
        )
        self._context = np.zeros(
            (1, CONTEXT_SIZE),
            dtype=np.float32,
        )
        self._sr = np.array(
            SAMPLE_RATE,
            dtype=np.int64,
        )

    def _reset_model(self):
        self._state.fill(0)
        self._context.fill(0)

    def reset_utterance(self):
        self.buffer = []
        self.recording = False
        self.silence_count = 0
        self.speech_prob = 0.0
        self.pre_buffer.clear()
        self._reset_model()

    def reset_all(self):
        self.pending = np.empty(0, dtype=np.int16)
        self.reset_utterance()

    def _infer(self, pcm: np.ndarray) -> float:
        pcm_f32 = pcm.astype(np.float32) / 32768.0
        x = pcm_f32.reshape(1, FRAME_SIZE)

        x_ctx = np.concatenate(
            [self._context, x],
            axis=1,
        )

        inputs = {
            "input": x_ctx,
            "state": self._state,
            "sr": self._sr,
        }

        out, self._state = self.session.run(
            None,
            inputs,
        )

        self._context = x_ctx[:, -CONTEXT_SIZE:]
        self.speech_prob = float(out[0, 0])

        return self.speech_prob

    def _finish(self):
        if not self.buffer:
            self.reset_utterance()
            return None

        audio = np.concatenate(self.buffer).astype(
            np.int16,
            copy=False,
        )

        self.reset_utterance()

        if len(audio) < self.min_samples:
            return None

        return audio

    def process_frame(self, pcm: np.ndarray):
        if len(pcm) != FRAME_SIZE:
            raise ValueError(
                f"VAD frame must be {FRAME_SIZE} samples"
            )

        prob = self._infer(pcm)

        if not self.recording:
            self.pre_buffer.append(pcm.copy())

            if prob >= self.threshold:
                self.recording = True
                self.buffer.extend(self.pre_buffer)
                self.pre_buffer.clear()
                self.silence_count = 0

            return None

        self.buffer.append(pcm.copy())

        if prob < self.neg_threshold:
            self.silence_count += 1
        else:
            self.silence_count = 0

        total_samples = sum(len(x) for x in self.buffer)

        if self.silence_count >= self.silence_frames:
            return self._finish()

        if total_samples >= self.max_samples:
            return self._finish()

        return None

    def feed(self, pcm: np.ndarray):
        """
        Accept arbitrary packet sizes.

        Returns zero or more completed utterances.
        Internally Silero always receives exactly 512 samples.
        """
        pcm = np.asarray(
            pcm,
            dtype=np.int16,
        ).reshape(-1)

        if not len(pcm):
            return []

        if len(self.pending):
            pcm = np.concatenate(
                [self.pending, pcm]
            )

        utterances = []
        pos = 0

        while pos + FRAME_SIZE <= len(pcm):
            frame = pcm[
                pos:pos + FRAME_SIZE
            ]

            utterance = self.process_frame(frame)

            if utterance is not None:
                utterances.append(utterance)

            pos += FRAME_SIZE

        self.pending = pcm[pos:].copy()

        return utterances

    def flush(self):
        """
        Safety flush for a broken/stopped UDP stream.
        This is NOT normal speech endpointing.
        """
        if not self.recording:
            self.pending = np.empty(
                0,
                dtype=np.int16,
            )
            return None

        if len(self.pending):
            self.buffer.append(
                self.pending.copy()
            )

        self.pending = np.empty(
            0,
            dtype=np.int16,
        )

        return self._finish()
