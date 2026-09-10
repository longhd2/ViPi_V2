#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-/home/server/ViPi_V2}"
[ "$TARGET" != "/home/server/ViPi" ] || { echo "REFUSE: V1 must remain untouched"; exit 2; }
if [ "$ROOT" != "$TARGET" ]; then
  [ ! -e "$TARGET" ] || { echo "REFUSE: $TARGET already exists"; exit 3; }
  cp -a "$ROOT" "$TARGET"
fi
cd "$TARGET"
python3 -m venv .venv
.venv/bin/pip install -U pip
.venv/bin/pip install -r install/requirements.txt
mkdir -p tmp/{audio,media,tts,cache}
./tools/check_no_llm.sh
python3 -m compileall -q Server
printf '\nInstalled ViPi_V2 at %s\nV1 untouched: /home/server/ViPi\n' "$TARGET"

# Install/refresh ViPi V2 system sounds when network + ffmpeg are available.
if command -v curl >/dev/null 2>&1 && command -v ffmpeg >/dev/null 2>&1; then
  "$DEST/install/download_sounds.sh" || echo "WARNING: sound download failed; run $DEST/install/download_sounds.sh later"
else
  echo "INFO: curl/ffmpeg unavailable; run install/download_sounds.sh after installing them"
fi
