# ViPi V2 - Test Report

Cấu trúc đã rút gọn theo quyết định cuối:
- `Server/src/...` -> `Server/...`
- `Client/...` -> `Client/...`

## PASS trên môi trường host
- Python compile toàn bộ Server
- Understanding Engine / room context
- Skill Registry
- HA child-skill graceful failure
- Media/timer/utility routing
- No-LLM guard
- Audio priority logic
- TTS gain logic
- Dynamic LED count / volume preview
- Touch action routing
- Mic mute state
- GPIO conflict validation

## Chưa thể xác nhận nếu chưa build/nạp ESP32 thật
- ESP-IDF/ESPHome compile với hardware runtime hoàn chỉnh
- I2S RX/TX thực tế
- Opus TTS TCP end-to-end
- AAC/M4A decoder/proxy end-to-end
- Wake word MultiNet thực tế
- WS2812/SK6812 RMT thực tế
- 4 touch pad calibration thực tế
- Web/NVS/OTA thực tế

Không được coi các mục phần cứng trên là PASS trước khi compile/nạp/test trên board.
