# 🔧 ViPi V2 - Hướng dẫn ESP32

## 1. Thư mục Client

```bash
clear
cd $HOME/ViPi_V2/Client
```

Không sử dụng `$HOME/ViPi_V2_NEW`.

Không tạo cố định `Client/components/`. ViPi sử dụng `runtime_components` theo cấu trúc hiện tại.

---

## 2. Biên dịch ESP32

Lệnh chuẩn:

```bash
clear
cd $HOME/ViPi_V2/Client
./build.sh
```

`build.sh` thực hiện quá trình biên dịch theo cấu hình Client hiện tại.

Sau khi thành công, kiểm tra:

```bash
clear
cd $HOME/ViPi_V2/Client
find . -maxdepth 1 -type f \( -name 'VIPI*_CLIENT_APP.bin' -o -name 'VIPI*_CLIENT_FULL.bin' -o -name '*.elf' \) -printf '%f  %s bytes\n' | sort
```

---

## 3. Hai loại firmware

### CLIENT_APP.bin

```text
VIPI_<VERSION>_CLIENT_APP.bin
```

Đây là firmware ứng dụng.

Dùng cho:

```text
OTA
```

### CLIENT_FULL.bin

```text
VIPI_<VERSION>_CLIENT_FULL.bin
```

Đây là firmware đầy đủ.

Dùng cho:

```text
Nạp mới ESP32 qua USB
Phục hồi ESP32
```

---

## 4. Ghép firmware

ViPi hiện sử dụng `Client/build.sh` để thực hiện quy trình build và tạo file firmware cuối.

Lệnh chuẩn:

```bash
clear
cd $HOME/ViPi_V2/Client
./build.sh
```

Nếu kết quả có cả:

```text
VIPI_<VERSION>_CLIENT_APP.bin
VIPI_<VERSION>_CLIENT_FULL.bin
```

thì quá trình build và tạo firmware đã hoàn tất.

Không tự đặt offset và ghép firmware bằng lệnh khác khi chưa kiểm tra cấu hình ESPHome/ESP-IDF thực tế.

---

## 5. Kiểm tra firmware mới nhất

```bash
clear
cd $HOME/ViPi_V2/Client
APP="$(find . -maxdepth 1 -type f -name 'VIPI*_CLIENT_APP.bin' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)"
FULL="$(find . -maxdepth 1 -type f -name 'VIPI*_CLIENT_FULL.bin' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)"
echo "APP  : $APP"
echo "FULL : $FULL"
[ -n "$APP" ] && ls -lh "$APP"
[ -n "$FULL" ] && ls -lh "$FULL"
```

---

## 6. Nạp FULL qua USB

Xác định cổng:

```bash
clear
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
```

Ví dụ ESP32 ở `/dev/ttyACM0`:

```bash
clear
cd $HOME/ViPi_V2/Client
FULL="$(find . -maxdepth 1 -type f -name 'VIPI*_CLIENT_FULL.bin' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)"
echo "Nạp firmware: $FULL"
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 write-flash 0x0 "$FULL"
```

Nếu thiết bị là `/dev/ttyUSB0`, thay cổng tương ứng.

---

## 7. OTA

OTA sử dụng:

```text
VIPI_<VERSION>_CLIENT_APP.bin
```

Không sử dụng `CLIENT_FULL.bin` cho OTA.

Tìm APP mới nhất:

```bash
clear
cd $HOME/ViPi_V2/Client
APP="$(find . -maxdepth 1 -type f -name 'VIPI*_CLIENT_APP.bin' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)"
echo "Firmware OTA: $APP"
ls -lh "$APP"
```

Trước OTA:

- ESP32 phải đang online.
- ESP32 và Server phải liên lạc được.
- Chỉ chọn `CLIENT_APP.bin`.
- Không ngắt nguồn trong quá trình OTA.

Nếu OTA qua Web Admin ViPi, chọn file APP vừa build và đúng ESP32 cần cập nhật.

---

## 8. Kiểm tra ESP32 trước cập nhật

Ví dụ:

```bash
clear
ping -c 3 192.168.1.7
```

Thay IP bằng ESP32 thực tế.

---

## 9. Kiểm tra sau cập nhật

Kiểm tra kết nối:

```bash
clear
ss -tnp | grep ':3001' || true
```

Kiểm tra dịch vụ:

```bash
clear
ss -lntup | grep -E ':3000|:3001|:3002|:8088|:8090|:8100'
```

Thử:

```text
Wake
STT
TTS
Media
Multi-intent
Home Assistant
```

---

## 10. Quy trình firmware chuẩn

```text
Sửa Client
   ↓
./build.sh
   ↓
APP.bin + FULL.bin
   ↓
Kiểm tra file
   ↓
Test một ESP32
   ↓
APP.bin → OTA
hoặc
FULL.bin → USB
   ↓
ESP32 kết nối lại
   ↓
Test Wake/STT/TTS/Media
   ↓
Ổn định
   ↓
Commit Git
```

Không đưa `.bin`, `.elf` và `.esphome` lên GitHub.
