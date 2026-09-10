# 💾 ViPi V2 - Hướng dẫn Git và GitHub

Repository:

```text
longhd2/ViPi_V2
```

Repository phải giữ ở chế độ **PRIVATE**.

---

## 1. Kiểm tra thay đổi

```bash
clear
cd $HOME/ViPi_V2
git status --short
git diff --stat
```

Xem chi tiết:

```bash
clear
cd $HOME/ViPi_V2
git diff
```

---

## 2. Đưa code đang phát triển lên GitHub

Sau khi test ổn:

```bash
clear
cd $HOME/ViPi_V2
git add -A
git status --short
git commit -m "Cập nhật ViPi"
git push origin main
```

---

## 3. Tạo phiên bản Stable mới

Ví dụ tạo `v2.0.2`:

```bash
clear
cd $HOME/ViPi_V2
printf '2.0.2\n' > VERSION
git add -A
git status --short
git commit -m "ViPi V2.0.2 Stable"
git push origin main
git tag -a v2.0.2 -m "ViPi V2.0.2 Stable"
git push origin v2.0.2
```

---

## 4. Kiểm tra Server và GitHub

```bash
clear
cd $HOME/ViPi_V2
echo "LOCAL HEAD  : $(git rev-parse --short HEAD)"
echo "REMOTE HEAD : $(git ls-remote origin refs/heads/main | cut -c1-7)"
echo "TAG         : $(git describe --tags --exact-match HEAD 2>/dev/null || true)"
```

LOCAL HEAD và REMOTE HEAD phải giống nhau.

---

## 5. Xem các phiên bản

```bash
clear
cd $HOME/ViPi_V2
git tag --sort=-version:refname
```

---

## 6. Xem lịch sử

```bash
clear
cd $HOME/ViPi_V2
git log --oneline --decorate --graph -20
```

---

## 7. Kiểm tra repository Private

```bash
clear
gh repo view longhd2/ViPi_V2 --json nameWithOwner,visibility,url
```

Phải hiển thị `PRIVATE`.

---

## 8. Khôi phục trên Server mới

Đăng nhập GitHub:

```bash
clear
gh auth login
```

Clone:

```bash
clear
cd /home/server
gh repo clone longhd2/ViPi_V2 ViPi_V2
cd $HOME/ViPi_V2
git status
```

Sau đó thực hiện theo `INSTALL.md`.

---

## 9. Xem bản Stable cũ

Ví dụ:

```bash
clear
cd $HOME/ViPi_V2
git switch --detach v2.0.0
```

Quay lại main:

```bash
clear
cd $HOME/ViPi_V2
git switch main
```

Không xóa hoặc di chuyển Stable tag cũ.

---

## 10. Không đưa lên GitHub

```text
Client/.esphome/
Client/*.bin
Client/*.elf
Server/cache/
logs/
backups/
Server/config/secrets.yaml
Server/data/clients.json
Server/data/scheduler.json
*.bak*
```

Không commit token, mật khẩu, API key hoặc thông tin xác thực.

---

## 11. Quy tắc phiên bản

ViPi sử dụng ba số:

```text
MAJOR.MINOR.PATCH
```

Ví dụ:

```text
2.0.0
2.0.1
2.0.2
2.1.0
3.0.0
```

Không sử dụng phiên bản bốn số như `2.0.0.1`.

---

## 12. Quy trình phát triển

```text
Stable
  ↓
main
  ↓
Sửa code
  ↓
Test
  ↓
git status / git diff
  ↓
Commit
  ↓
Push main
  ↓
Kiểm tra ổn định
  ↓
Tăng VERSION
  ↓
Tạo tag Stable
  ↓
Push tag
```
