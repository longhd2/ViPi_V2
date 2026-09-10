# 🎙️ ViPi V2

**Trợ lý giọng nói tiếng Việt chạy cục bộ, theo kiến trúc Skill, hỗ trợ nhiều ESP32 và không sử dụng LLM**

**Phiên bản:** `2.0.1`  
**Máy chủ:** Debian / Python  
**Thiết bị:** ESP32-S3 / ESPHome  
**Kho mã nguồn:** GitHub Private

---

## 1. Giới thiệu

ViPi V2 là hệ thống trợ lý giọng nói tiếng Việt gồm Server trung tâm và các ESP32-S3 làm thiết bị vệ tinh giọng nói.

Nguyên tắc thiết kế:

- Không sử dụng LLM.
- Một câu nói được Understanding Engine phân tích một lần.
- Intent được định tuyến trực tiếp tới Skill bằng chỉ mục trong RAM.
- Skill tự động được phát hiện khi Server khởi động.
- Hỗ trợ nhiều intent trong một câu.
- Hỗ trợ nhiều ESP32.
- Home Assistant chỉ là một Skill độc lập, không phải lõi hệ thống.
- Nếu Home Assistant ngừng hoạt động, STT, TTS, Media, Timer, Reminder, Weather và Utility vẫn có thể hoạt động.

---

## 2. Kiến trúc

```text
ESP32-S3
   ↓
STT + VAD
   ↓
ViPi Understanding Engine
 ├─ Chuẩn hóa
 ├─ Pattern Matcher
 ├─ Alias / Synonym
 ├─ Fuzzy Matching
 ├─ Semantic Matching nhẹ
 └─ Context
       ↓
Intent + Slots + Confidence
       ↓
Skill Registry
 ├─ Home Assistant
 ├─ Media
 ├─ Timer / Alarm
 ├─ Reminder
 ├─ Scheduled Action
 ├─ Weather
 ├─ Utility
 ├─ Calculator
 ├─ Location
 ├─ News
 └─ System
       ↓
      TTS
       ↓
     ESP32
```

ViPi V2 không sử dụng Ollama, Qwen, Gemini, OpenClaw hoặc LLM fallback.

Khi độ tin cậy thấp, ViPi ưu tiên hỏi lại thay vì đoán.

---

## 3. Understanding Engine và Skill Registry

Mỗi câu nói chỉ được phân tích một lần.

Routing:

```text
intent_name → chỉ mục trong RAM → Skill
```

Không duyệt tuần tự toàn bộ Skill cho mỗi yêu cầu.

Hệ thống hỗ trợ:

- Chuẩn hóa câu nói.
- Phrase/pattern matching.
- Alias và synonym.
- Fuzzy matching.
- Context.
- Slot extraction.
- Confidence.
- Multi-intent.
- Exclusive intent.

---

## 4. Multi-intent

Ví dụ:

```text
mấy giờ rồi và phát nhạc Nơi Này Có Anh
```

Có thể tạo:

```text
utility.time
media.play
```

Các phản hồi TTS được phát trước. `media.play` được thực hiện sau cùng để hạn chế tranh chấp audio.

---

## 5. Home Assistant

Home Assistant là một Skill của ViPi.

Các domain được thiết kế hỗ trợ:

```text
light
switch
fan
climate
cover
script
scene
automation
sensor
binary_sensor
```

Ưu tiên nhận diện:

```text
friendly_name → aliases → Area → entity_id
```

Ví dụ:

```text
bật đèn ngủ
tắt đèn bàn
tắt đèn phòng khách
bật điều hòa phòng ngủ
mở rèm phòng khách
kiểm tra đèn ngủ
kiểm tra điện phòng ngủ
```

State Guard tránh gửi service call thừa nếu thiết bị đã ở đúng trạng thái.

---

## 6. Media

```text
Giọng nói → media.play → Media Skill → Media Manager → HTTP Media Proxy → ESP32
```

Media Proxy sử dụng TCP 8088.

---

## 7. Timer, Reminder và Scheduler

Ví dụ:

```text
hẹn giờ 10 phút
các hẹn giờ
hủy hẹn giờ
nhắc tôi uống nước sau 10 phút
10 phút nữa tắt nhạc
10 phút sau phát bài Hai Mùa Mưa
```

Dữ liệu Scheduler khi chạy:

```text
Server/data/scheduler.json
```

Không lưu dữ liệu runtime này vào Git.

---

## 8. STT, VAD và TTS

Nhận giọng nói:

```text
ESP32 MIC → UDP 3000 → VAD → STT → Understanding
```

Phát giọng nói:

```text
Text → TTS → TCP 3001 → ESP32 Speaker
```

TTS có chuẩn hóa số thập phân tiếng Việt, ví dụ:

```text
28.5 → 28 phẩy 5
```

---

## 9. Web Admin

```text
http://SERVER_IP:8090
```

Mạng hiện tại:

```text
http://192.168.1.222:8090
```

---

## 10. Cổng mạng

| Cổng | Giao thức | Chức năng |
|---:|---|---|
| 3000 | UDP | Microphone |
| 3001 | TCP | TTS / loa ESP32 |
| 3002 | TCP | Điều khiển |
| 8088 | TCP | Media Proxy |
| 8090 | TCP | Web Admin |
| 8100 | TCP | STT nội bộ |

Không tự ý đóng TCP 3001 khi hệ thống đang hoạt động.

---

## 11. Firmware ESP32

Quy trình:

```text
Source Client
    ↓
./build.sh
    ↓
Biên dịch
    ├─ VIPI_<VERSION>_CLIENT_APP.bin
    └─ VIPI_<VERSION>_CLIENT_FULL.bin
```

- `CLIENT_APP.bin`: dùng cập nhật OTA.
- `CLIENT_FULL.bin`: dùng nạp mới hoặc phục hồi ESP32 qua USB.

Chi tiết xem `ESP32_GUIDE.md`.

---

## 12. Git và GitHub

Quy trình:

```text
Phát triển → Kiểm tra → Commit → Push main → Kiểm tra ổn định → Tạo tag Stable
```

Chi tiết xem `GIT_GUIDE.md`.

---

## 13. Tài liệu

| Tệp | Nội dung |
|---|---|
| `README.md` | Tổng quan ViPi |
| `RUN_GUIDE.md` | Chạy và kiểm tra Server |
| `ESP32_GUIDE.md` | Build, firmware, USB và OTA |
| `GIT_GUIDE.md` | Git, GitHub, version và phục hồi |
| `INSTALL.md` | Cài đặt/khôi phục |
| `VERSION` | Phiên bản hiện tại |

---

## 14. Phiên bản

```text
v2.0.0  Bản Stable ban đầu
v2.0.1  Tài liệu tiếng Việt, hướng dẫn vận hành, ESP32 và Git
```

Từ đây ViPi chỉ sử dụng phiên bản ba số:

```text
2.0.1 → 2.0.2 → 2.0.3 → 2.1.0
```

---

## 15. Bảo mật

Không đưa lên GitHub:

- `Server/config/secrets.yaml`
- Home Assistant token
- Mật khẩu MQTT
- API key
- `.esphome`
- `.bin`
- `.elf`
- cache
- logs
- backup runtime
- scheduler runtime

**ViPi V2.0.1**
