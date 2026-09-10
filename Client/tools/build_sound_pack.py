#!/usr/bin/env python3
import os
import sys
import wave
import struct
import hashlib

SOUND_DIR = "/home/server/ViPi_V2/sounds"
OUT_FILE = "/home/server/ViPi_V2/Client/sound_pack.bin"

MAGIC = b"VPISND2\x00"
VERSION = 2
HEADER_SIZE = 32
ENTRY_SIZE = 80
MAX_STORAGE = 0x7F0000

files = sorted(
    f for f in os.listdir(SOUND_DIR)
    if f.lower().endswith(".wav")
)

if not files:
    raise SystemExit("LOI: Khong tim thay file WAV")

validated = []

print("===== KIEM TRA WAV =====")
for name in files:
    path = os.path.join(SOUND_DIR, name)

    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        rate = w.getframerate()
        width = w.getsampwidth()
        frames = w.getnframes()

    size = os.path.getsize(path)

    if ch != 1:
        raise SystemExit(f"LOI {name}: channels={ch}, can mono")
    if rate != 16000:
        raise SystemExit(f"LOI {name}: rate={rate}, can 16000Hz")
    if width != 2:
        raise SystemExit(f"LOI {name}: sample width={width}, can 16-bit")

    validated.append((name, path, size))
    print(f"OK  {name:40s} {size:8d} bytes  PCM16 MONO 16000Hz")

count = len(validated)
data_start = HEADER_SIZE + count * ENTRY_SIZE

# Canh data theo 4 byte
data_start = (data_start + 3) & ~3

entries = []
offset = data_start

for name, path, size in validated:
    entries.append((name, offset, size))
    offset += size
    offset = (offset + 3) & ~3

total_size = offset

if total_size > MAX_STORAGE:
    raise SystemExit(
        f"LOI: Sound pack {total_size} > storage {MAX_STORAGE}"
    )

image = bytearray(b"\xFF" * total_size)

# Header:
# magic[8], version u32, count u32, entry_size u32,
# data_start u32, total_size u32, reserved u32
struct.pack_into(
    "<8sIIIIII",
    image,
    0,
    MAGIC,
    VERSION,
    count,
    ENTRY_SIZE,
    data_start,
    total_size,
    0,
)

for i, (name, file_offset, size) in enumerate(entries):
    encoded = name.encode("utf-8")

    if len(encoded) > 63:
        raise SystemExit(f"LOI: ten file qua dai: {name}")

    base = HEADER_SIZE + i * ENTRY_SIZE

    name_field = encoded + b"\x00" * (64 - len(encoded))

    struct.pack_into(
        "<64sIIII",
        image,
        base,
        name_field,
        file_offset,
        size,
        0,
        0,
    )

    with open(os.path.join(SOUND_DIR, name), "rb") as f:
        raw = f.read()

    image[file_offset:file_offset + size] = raw

with open(OUT_FILE, "wb") as f:
    f.write(image)

sha = hashlib.sha256(image).hexdigest()

print()
print("===== SOUND PACK =====")
print("Files       :", count)
print("Size        :", total_size, "bytes")
print("Storage max :", MAX_STORAGE, "bytes")
print("Free        :", MAX_STORAGE - total_size, "bytes")
print("SHA256      :", sha)
print("Output      :", OUT_FILE)
