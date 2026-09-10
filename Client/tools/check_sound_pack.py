#!/usr/bin/env python3
import struct
import sys
import os

path = "/home/server/ViPi_V2/Client/sound_pack.bin"

with open(path, "rb") as f:
    data = f.read()

magic, version, count, entry_size, data_start, total_size, reserved = \
    struct.unpack_from("<8sIIIIII", data, 0)

assert magic == b"VPISND2\x00"
assert version == 2
assert entry_size == 80
assert total_size == len(data)

print("===== VERIFY SOUND PACK =====")
print("Magic :", magic)
print("Version:", version)
print("Count :", count)
print("Size  :", total_size)
print()

for i in range(count):
    base = 32 + i * 80
    name_raw, offset, size, flags, reserved = \
        struct.unpack_from("<64sIIII", data, base)

    name = name_raw.split(b"\x00", 1)[0].decode()

    assert offset + size <= len(data)

    wav = data[offset:offset + 4]
    assert wav == b"RIFF", name

    print(
        f"{i+1:02d}. "
        f"{name:40s} "
        f"offset=0x{offset:06X} "
        f"size={size}"
    )

print()
print("VERIFY: PASS")
