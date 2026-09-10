#!/usr/bin/env bash
set -euo pipefail
cd /home/server/ViPi_V2
./tools/check_no_llm.sh
.venv/bin/pip install -r install/requirements.txt
python3 -m compileall -q Server
if command -v systemctl >/dev/null 2>&1; then sudo systemctl restart vipi-v2.service || true; fi
