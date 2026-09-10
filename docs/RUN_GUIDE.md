# 🚀 ViPi V2 - Hướng dẫn vận hành Server

## Kiểm tra phiên bản

```bash
clear
cd $HOME/ViPi_V2
cat VERSION
git log -1 --oneline
git describe --tags --always
```

## Kiểm tra Server

```bash
clear
ps aux | grep '[p]ython.*Server/main.py'
```

## Kiểm tra các cổng

```bash
clear
ss -lntup | grep -E ':3000|:3001|:3002|:8088|:8090|:8100'
```

## Chạy Server thủ công

```bash
clear
cd $HOME/ViPi_V2
$HOME/.env/bin/python -m Server.main
```

Nếu Supervisor đang quản lý ViPi, không chạy thêm Server thứ hai.

## Test Understanding/Home Assistant bằng văn bản

```bash
clear
cd $HOME/ViPi_V2
$HOME/.env/bin/python Server/tools/test_voice.py
```

## Web Admin

```text
http://192.168.1.222:8090
```

## Kiểm tra Git

```bash
clear
cd $HOME/ViPi_V2
git status --short --branch
git log --oneline --decorate -10
git tag --sort=-version:refname
```

## Lưu ý

- Không sửa V1 tại `$HOME/ViPi`.
- V2 nằm tại `$HOME/ViPi_V2`.
- Không đóng TCP 3001.
- Không đưa secret lên GitHub.
- Hướng dẫn firmware ESP32 xem `Hardware/ESP32_GUIDE.md`.
