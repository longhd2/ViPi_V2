#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YAML="$ROOT/vipi-v2.yaml"
BUILD="$ROOT/.esphome/build/vipi-v2"
ESPHOME="${ESPHOME:-$HOME/esphome-env/bin/esphome}"

FLASH_SIZE=$((0x1000000))

cd "$ROOT"

CURRENT_VERSION="$(cat "$ROOT/VERSION")"
NEXT_VERSION="$CURRENT_VERSION"

export VIPI_CLIENT_BUILD_VERSION="$NEXT_VERSION"

echo
echo "===== CLIENT VERSION ====="
echo "Build   : $NEXT_VERSION"

PARTITION_INFO="$(python3 -c '
import csv
from pathlib import Path
p=Path("partitions.csv")
rows={}
with p.open() as f:
    for r in csv.reader(x for x in f if x.strip() and not x.lstrip().startswith("#")):
        if len(r)>=5: rows[r[0].strip()]=(int(r[3],0),int(r[4],0))
print(rows["ota_0"][1],rows["storage"][0],rows["storage"][1])
')"
read -r APP_MAX STORAGE_OFFSET STORAGE_SIZE <<< "$PARTITION_INFO"
export ROOT APP_MAX STORAGE_OFFSET STORAGE_SIZE FLASH_SIZE

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
    false
fi

if [ ! -f "$ROOT/sound_pack.bin" ]; then
    echo "LOI: Khong tao duoc sound_pack.bin"
    false
fi

SOUND_SIZE=$(stat -c%s "$ROOT/sound_pack.bin")

if [ "$SOUND_SIZE" -gt "$STORAGE_SIZE" ]; then
    echo "LOI: Sound pack vuot storage"
    false
fi

echo "Sound size    : $SOUND_SIZE bytes"
echo "Storage size  : $STORAGE_SIZE bytes"
printf "Storage offset: 0x%X\n" "$STORAGE_OFFSET"

echo
echo "===== 3. BUILD VERSION ====="

python3 "$ROOT/tools/generate_version.py"

echo
echo "===== 4. COMPILE FIRMWARE ====="

"$ESPHOME" compile "$YAML"

echo
echo "===== 4. TIM FILE BUILD ====="

FACTORY="$(find "$BUILD" -type f -name 'firmware.factory.bin' | head -1)"
OTA="$(find "$BUILD" -type f -name 'firmware.ota.bin' | head -1)"
ELF="$(find "$BUILD" -type f -name 'firmware.elf' | head -1)"

if [ -z "$FACTORY" ] || [ ! -f "$FACTORY" ]; then
    echo "LOI: Khong tim thay firmware.factory.bin"
    false
fi

if [ -z "$OTA" ] || [ ! -f "$OTA" ]; then
    echo "LOI: Khong tim thay firmware.ota.bin"
    false
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
    false
fi

echo
echo "===== 7. TAO FLASH ALL 16MB ====="

python3 - <<'PY'
from pathlib import Path
import os

root = Path(os.environ["ROOT"])

factory_path = root / "VIPI_V2_CLIENT_FULL.bin"
sound_path   = root / "VIPI_V2_SOUNDS.bin"
output_path  = root / "VIPI_V2_FLASH_ALL.bin"

FLASH_SIZE = int(os.environ.get("FLASH_SIZE", "0x1000000"), 0)
STORAGE_OFFSET = int(os.environ["STORAGE_OFFSET"])
STORAGE_SIZE = int(os.environ["STORAGE_SIZE"])

factory = factory_path.read_bytes()
sound = sound_path.read_bytes()

if len(factory) > STORAGE_OFFSET:
    raise RuntimeError(
        "LOI: factory.bin cham vao storage "
        f"({len(factory)} > {STORAGE_OFFSET})"
    )

if len(sound) > STORAGE_SIZE:
    raise RuntimeError(
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
    raise RuntimeError("LOI: FLASH_ALL khong dung 16MB")

print("FLASH ALL CHECK: PASS")
PY

echo
echo "===== 8. KIEM TRA SOUND TRONG FLASH ALL ====="

python3 - <<'PY'
from pathlib import Path
import os

root = Path(os.environ["ROOT"])

all_data = (root / "VIPI_V2_FLASH_ALL.bin").read_bytes()
sound = (root / "VIPI_V2_SOUNDS.bin").read_bytes()

offset = int(os.environ["STORAGE_OFFSET"])

embedded = all_data[offset:offset + len(sound)]

if embedded != sound:
    raise RuntimeError(
        "LOI: Sound trong FLASH_ALL khong trung khop"
    )

if embedded[:8] != b"VPISND2\x00":
    raise RuntimeError(
        "LOI: Khong tim thay ViPi Sound Pack tai storage partition"
    )

print(f"Sound offset : 0x{offset:X}")
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
echo
echo "===== 9B. TAO OTA RELEASE ====="
python3 "$ROOT/tools/create_release.py"
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
echo "===== CLIENT VERSION ====="
echo "Version: $(cat "$ROOT/VERSION")"

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
