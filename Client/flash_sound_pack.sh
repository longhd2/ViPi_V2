#!/usr/bin/env bash
set -e

ROOT="/home/server/ViPi_V2/Client"
PACK="$ROOT/sound_pack.bin"
OFFSET="0x810000"

echo "===== VIPI V2 SOUND PACK FLASH ====="
echo "File   : $PACK"
echo "Offset : $OFFSET"
echo

if [ ! -f "$PACK" ]; then
    echo "LOI: Khong co sound_pack.bin"
    exit 1
fi

PORT="${1:-}"

if [ -z "$PORT" ]; then
    echo "Cach dung:"
    echo "  ./flash_sound_pack.sh /dev/ttyACM0"
    echo "hoac"
    echo "  ./flash_sound_pack.sh /dev/ttyUSB0"
    exit 1
fi

ESPHOME="/home/server/esphome-env/bin/esphome"

echo "CHI FLASH SOUND STORAGE - KHONG FLASH FIRMWARE"
echo "Port: $PORT"
echo

python3 -m esptool \
    --chip esp32s3 \
    --port "$PORT" \
    write_flash \
    "$OFFSET" \
    "$PACK"
