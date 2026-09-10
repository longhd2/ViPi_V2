#!/usr/bin/env bash
set -e

ROOT="/home/server/ViPi_V2/Client"
YAML="$ROOT/vipi-v2.yaml"
BUILD="$ROOT/.esphome/build/vipi-v2"
ESPHOME="/home/server/esphome-env/bin/esphome"

FLASH_SIZE=$((0x1000000))
STORAGE_OFFSET=$((0xA10000))
STORAGE_SIZE=$((0x5F0000))
APP_MAX=$((0x380000))

cd "$ROOT"

echo
echo "=================================================="
echo "                VIPI V2 BUILD"
echo "=================================================="

echo
echo "===== 1. PARTITION ====="
cat partitions.csv

echo
echo "===== 2. TAO SOUND PACK ====="

if [ -f "$ROOT/tools/build_sound_pack.py" ]; then
    python3 "$ROOT/tools/build_sound_pack.py"
else
    echo "LOI: Khong co tools/build_sound_pack.py"
    exit 1
fi

if [ ! -f "$ROOT/sound_pack.bin" ]; then
    echo "LOI: Khong tao duoc sound_pack.bin"
    exit 1
fi

SOUND_SIZE=$(stat -c%s "$ROOT/sound_pack.bin")

if [ "$SOUND_SIZE" -gt "$STORAGE_SIZE" ]; then
    echo "LOI: Sound pack vuot storage"
    exit 1
fi

echo "Sound size    : $SOUND_SIZE bytes"
echo "Storage size  : $STORAGE_SIZE bytes"
echo "Storage offset: 0xA10000"

echo
echo "===== 3. COMPILE FIRMWARE ====="

"$ESPHOME" compile "$YAML"

echo
echo "===== 4. TIM FILE BUILD ====="

FACTORY="$(find "$BUILD" -type f -name 'firmware.factory.bin' | head -1)"
OTA="$(find "$BUILD" -type f -name 'firmware.ota.bin' | head -1)"
ELF="$(find "$BUILD" -type f -name 'firmware.elf' | head -1)"

if [ -z "$FACTORY" ] || [ ! -f "$FACTORY" ]; then
    echo "LOI: Khong tim thay firmware.factory.bin"
    exit 1
fi

if [ -z "$OTA" ] || [ ! -f "$OTA" ]; then
    echo "LOI: Khong tim thay firmware.ota.bin"
    exit 1
fi

echo "FACTORY: $FACTORY"
echo "OTA    : $OTA"

echo
echo "===== 5. XUAT FILE VIPI ====="

cp -f "$OTA" \
    "$ROOT/VIPI_V2_CLIENT_APP.bin"

cp -f "$FACTORY" \
    "$ROOT/VIPI_V2_CLIENT_FULL.bin"

cp -f "$ROOT/sound_pack.bin" \
    "$ROOT/VIPI_V2_SOUNDS.bin"

if [ -n "$ELF" ] && [ -f "$ELF" ]; then
    cp -f "$ELF" \
        "$ROOT/VIPI_V2_CLIENT.elf"
fi

echo
echo "===== 6. KIEM TRA APP ====="

APP_SIZE=$(stat -c%s "$ROOT/VIPI_V2_CLIENT_APP.bin")
APP_FREE=$((APP_MAX - APP_SIZE))
APP_PERCENT=$((APP_SIZE * 100 / APP_MAX))

echo "APP used : $APP_SIZE bytes"
echo "APP max  : $APP_MAX bytes"
echo "APP free : $APP_FREE bytes"
echo "APP usage: ${APP_PERCENT}%"

if [ "$APP_SIZE" -gt "$APP_MAX" ]; then
    echo "LOI: APP VUOT OTA PARTITION"
    exit 1
fi

echo
echo "===== 7. TAO FLASH ALL 16MB ====="

python3 - <<'PY'
from pathlib import Path

root = Path("/home/server/ViPi_V2/Client")

factory_path = root / "VIPI_V2_CLIENT_FULL.bin"
sound_path   = root / "VIPI_V2_SOUNDS.bin"
output_path  = root / "VIPI_V2_FLASH_ALL.bin"

FLASH_SIZE = 0x1000000
STORAGE_OFFSET = 0xA10000
STORAGE_SIZE = 0x5F0000

factory = factory_path.read_bytes()
sound = sound_path.read_bytes()

if len(factory) > STORAGE_OFFSET:
    raise SystemExit(
        "LOI: factory.bin cham vao storage "
        f"({len(factory)} > {STORAGE_OFFSET})"
    )

if len(sound) > STORAGE_SIZE:
    raise SystemExit(
        "LOI: sound.bin vuot storage"
    )

# Tao image flash 16 MB rong = 0xFF.
image = bytearray(b"\xFF" * FLASH_SIZE)

# firmware.factory.bin cua ESPHome bat dau tai dia chi flash 0.
image[0:len(factory)] = factory

# ViPi Sound Pack nam dung partition storage.
sound_end = STORAGE_OFFSET + len(sound)
image[STORAGE_OFFSET:sound_end] = sound

output_path.write_bytes(image)

print(f"Factory       : {len(factory):,} bytes")
print(f"Sound         : {len(sound):,} bytes")
print(f"Sound offset  : 0x{STORAGE_OFFSET:06X}")
print(f"Sound end     : 0x{sound_end:06X}")
print(f"FLASH ALL     : {len(image):,} bytes")
print(f"FLASH END     : 0x{len(image):08X}")

if len(image) != FLASH_SIZE:
    raise SystemExit("LOI: FLASH_ALL khong dung 16MB")

print("FLASH ALL CHECK: PASS")
PY

echo
echo "===== 8. KIEM TRA SOUND TRONG FLASH ALL ====="

python3 - <<'PY'
from pathlib import Path

root = Path("/home/server/ViPi_V2/Client")

all_data = (root / "VIPI_V2_FLASH_ALL.bin").read_bytes()
sound = (root / "VIPI_V2_SOUNDS.bin").read_bytes()

offset = 0xA10000

embedded = all_data[offset:offset + len(sound)]

if embedded != sound:
    raise SystemExit(
        "LOI: Sound trong FLASH_ALL khong trung khop"
    )

if embedded[:8] != b"VPISND2\x00":
    raise SystemExit(
        "LOI: Khong tim thay ViPi Sound Pack tai 0xA10000"
    )

print("Sound offset : 0xA10000")
print("Magic        :", embedded[:8])
print("Sound verify : PASS")
PY

echo
echo "===== 9. SHA256 ====="

sha256sum \
    "$ROOT/VIPI_V2_CLIENT_APP.bin" \
    "$ROOT/VIPI_V2_CLIENT_FULL.bin" \
    "$ROOT/VIPI_V2_SOUNDS.bin" \
    "$ROOT/VIPI_V2_FLASH_ALL.bin"

echo
echo "===== 10. FIRMWARE VIPI V2 ====="

ls -lh \
    "$ROOT/VIPI_V2_CLIENT_APP.bin" \
    "$ROOT/VIPI_V2_CLIENT_FULL.bin" \
    "$ROOT/VIPI_V2_SOUNDS.bin" \
    "$ROOT/VIPI_V2_FLASH_ALL.bin" \
    "$ROOT/VIPI_V2_CLIENT.elf" \
    2>/dev/null || true

echo
echo "=================================================="
echo " VIPI V2 BUILD: SUCCESS"
echo
echo " LAN DAU:"
echo "   VIPI_V2_FLASH_ALL.bin"
echo
echo " CAP NHAT SAU NAY:"
echo "   VIPI_V2_CLIENT_APP.bin"
echo
echo " DOI BO AM THANH:"
echo "   VIPI_V2_SOUNDS.bin"
echo
echo " CHUA FLASH ESP32"
echo "=================================================="
