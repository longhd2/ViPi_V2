# ViPi_V2

Du an ViPi V2 doc lap voi `/home/server/ViPi`.

Cau truc rut gon:
- `Server/`: server ViPi V2, khong LLM.
- `Client/`: firmware ESP32; module nam truc tiep trong `audio/`, `core/`, `led/`, `touch/`, `wake/`, `network/`, `web/`, `ota/`, `sounds/`.
- `sounds/`: kho tep am thanh `.wav` dung chung; tat ca file nam truc tiep tai day, khong chia folder con.
- `tmp/`: audio/media/TTS/cache tam.
- `install/`: cai dat va service.
- `tools/`: build/test.

V1 khong bi sua doi.

## System sounds
`sounds/` is flat: WAV files live directly in this directory. Run `install/download_sounds.sh` to fetch the sound set referenced by Satellite1/Home Assistant Voice PE and normalize it to PCM 16-bit, 16 kHz, mono WAV. No nested sound folders are used.


## Optional display

ViPi V2 includes an optional `Client/display/` module. To keep maintenance
simple, only two display controllers are targeted: **ST7789** (default) and
**ILI9341**. Display support is optional and does not affect headless operation.
