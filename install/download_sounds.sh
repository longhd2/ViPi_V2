#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/sounds"
TMP="$ROOT/tmp/sound_download"
BASE="https://github.com/esphome/home-assistant-voice-pe/raw/dev/sounds"
mkdir -p "$OUT" "$TMP"
command -v curl >/dev/null || { echo 'Missing curl'; exit 1; }
command -v ffmpeg >/dev/null || { echo 'Missing ffmpeg: sudo apt install -y ffmpeg'; exit 1; }
files=(
 jack_connected.flac jack_disconnected.flac
 mute_switch_on.flac mute_switch_off.flac
 timer_finished.flac wake_word_triggered.flac
 center_button_press.flac center_button_double_press.flac
 center_button_triple_press.flac center_button_long_press.flac
 factory_reset_initiated.mp3 factory_reset_cancelled.mp3 factory_reset_confirmed.mp3
)
for f in "${files[@]}"; do
  echo "[ViPi V2] Downloading $f"
  curl -fL --retry 3 "$BASE/$f" -o "$TMP/$f"
  name="${f%.*}"
  ffmpeg -hide_banner -loglevel error -y -i "$TMP/$f" -ac 1 -ar 16000 -c:a pcm_s16le "$OUT/$name.wav"
done
rm -rf "$TMP"
echo "[ViPi V2] Sounds ready in $OUT"
file "$OUT"/*.wav || true
