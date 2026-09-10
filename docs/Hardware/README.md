# Tài liệu phần cứng ViPi V2

Đây là nơi tập trung toàn bộ tài liệu liên quan đến phần cứng và firmware ESP32 của ViPi.

## ESP32 và firmware

- [Biên dịch, ghép firmware, nạp USB và OTA](ESP32_GUIDE.md)

## Tài liệu phần cứng

Các tài liệu sau được đặt tại đây khi có:

- `PINOUT.md` — GPIO và sơ đồ chân.
- `AUDIO.md` — microphone, I2S, loa và mạch âm thanh.
- `LED.md` — LED WS2812 và trạng thái hiển thị.

## Quy ước

- Client chính: `/home/server/ViPi_V2/Client`
- Không sử dụng `/home/server/ViPi_V2_NEW`.
- Không tạo cố định `Client/components/`.
- Không đóng TCP 3001 khi ViPi đang hoạt động.
- `CLIENT_APP.bin` dùng cho OTA.
- `CLIENT_FULL.bin` dùng để nạp mới hoặc phục hồi ESP32 qua USB.
