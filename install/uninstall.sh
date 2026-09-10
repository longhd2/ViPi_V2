#!/usr/bin/env bash
set -euo pipefail
if command -v systemctl >/dev/null 2>&1; then
  sudo systemctl disable --now vipi-v2.service 2>/dev/null || true
  sudo rm -f /etc/systemd/system/vipi-v2.service
  sudo systemctl daemon-reload || true
fi
echo "Service removed. Project files in /home/server/ViPi_V2 are preserved."
