import socket
import time
import psutil
from .base import Skill, SkillResult


class SystemSkill(Skill):
    name = "system"
    priority = 80

    def _ip(self):
        sock = None
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.connect(("8.8.8.8", 80))
            return sock.getsockname()[0]
        except Exception:
            return "không xác định"
        finally:
            if sock:
                sock.close()

    def _temperature(self):
        try:
            temps = psutil.sensors_temperatures()
            for entries in temps.values():
                for entry in entries:
                    if entry.current:
                        return round(entry.current, 1)
        except Exception:
            pass
        return None

    def _uptime(self):
        seconds = int(time.time() - psutil.boot_time())
        days = seconds // 86400
        hours = (seconds % 86400) // 3600
        minutes = (seconds % 3600) // 60

        if days:
            return f"{days} ngày {hours} giờ"
        if hours:
            return f"{hours} giờ {minutes} phút"
        return f"{minutes} phút"

    async def handle(self, intent, ctx):
        text = intent.slots.get("text", "").lower()

        if "ip" in text:
            return SkillResult(
                True,
                f"Địa chỉ IP của server là {self._ip()}.",
            )

        if "uptime" in text or "chạy bao lâu" in text:
            return SkillResult(
                True,
                f"Server đã chạy liên tục {self._uptime()}.",
            )

        if "cpu" in text:
            cpu = psutil.cpu_percent(interval=0.2)
            temp = self._temperature()

            reply = f"CPU đang sử dụng {cpu:.0f} phần trăm"

            if temp is not None:
                reply += f", nhiệt độ khoảng {temp:.0f} độ C"

            return SkillResult(True, reply + ".")

        if "ram" in text or "bộ nhớ" in text:
            ram = psutil.virtual_memory()
            used = ram.used / 1024**3
            total = ram.total / 1024**3

            return SkillResult(
                True,
                f"RAM đang dùng {ram.percent:.0f} phần trăm, "
                f"khoảng {used:.1f} trên {total:.1f} GB.",
            )

        if (
            "ổ cứng" in text
            or "dung lượng ổ" in text
            or "dung lượng đĩa" in text
        ):
            disk = psutil.disk_usage("/")
            free = disk.free / 1024**3
            total = disk.total / 1024**3

            return SkillResult(
                True,
                f"Ổ cứng đang dùng {disk.percent:.0f} phần trăm. "
                f"Còn trống khoảng {free:.0f} GB trên "
                f"{total:.0f} GB.",
            )

        cpu = psutil.cpu_percent(interval=0.2)
        ram = psutil.virtual_memory()
        disk = psutil.disk_usage("/")

        return SkillResult(
            True,
            f"Server đang hoạt động bình thường. "
            f"CPU {cpu:.0f} phần trăm, "
            f"RAM {ram.percent:.0f} phần trăm, "
            f"ổ cứng {disk.percent:.0f} phần trăm.",
        )


MANIFEST = {
    "name": "system",
    "intents": {
        "system.status": {
            "phrases": [
                "server khỏe không",
                "server thế nào",
                "tình trạng server",
                "trạng thái server",
            ],
            "copy_text": True,
        },
        "system.cpu": {
            "phrases": [
                "cpu",
                "cpu server",
                "nhiệt độ cpu",
            ],
            "copy_text": True,
        },
        "system.memory": {
            "phrases": [
                "ram",
                "ram server",
                "bộ nhớ server",
            ],
            "copy_text": True,
        },
        "system.disk": {
            "phrases": [
                "ổ cứng",
                "dung lượng ổ",
                "dung lượng đĩa",
            ],
            "copy_text": True,
        },
        "system.ip": {
            "phrases": [
                "địa chỉ ip",
                "ip server",
                "ip máy",
            ],
            "copy_text": True,
        },
        "system.uptime": {
            "phrases": [
                "uptime",
                "server chạy bao lâu",
            ],
            "copy_text": True,
        },
    },
}
