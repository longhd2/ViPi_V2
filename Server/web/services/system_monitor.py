import os
import time
import shutil
import socket
import psutil

BOOT = time.time() - psutil.boot_time()

def _temperature():
    try:
        temps = psutil.sensors_temperatures()
        for group in temps.values():
            for item in group:
                if item.current is not None:
                    return round(float(item.current), 1)
    except Exception:
        pass

    for path in (
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
    ):
        try:
            with open(path, "r", encoding="utf-8") as f:
                value = float(f.read().strip())
                return round(value / 1000 if value > 200 else value, 1)
        except Exception:
            pass
    return None

def _lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("1.1.1.1", 80))
        return s.getsockname()[0]
    except Exception:
        return "N/A"
    finally:
        s.close()

def get_system_status():
    vm = psutil.virtual_memory()
    disk = shutil.disk_usage("/")
    return {
        "hostname": socket.gethostname(),
        "lan_ip": _lan_ip(),
        "cpu": round(psutil.cpu_percent(interval=0.15), 1),
        "cpu_count": psutil.cpu_count(),
        "load": [round(x, 2) for x in os.getloadavg()],
        "ram_percent": round(vm.percent, 1),
        "ram_used_gb": round((vm.total - vm.available) / 1073741824, 2),
        "ram_total_gb": round(vm.total / 1073741824, 2),
        "disk_percent": round(disk.used / disk.total * 100, 1),
        "disk_used_gb": round(disk.used / 1073741824, 1),
        "disk_total_gb": round(disk.total / 1073741824, 1),
        "temperature": _temperature(),
        "uptime": int(time.time() - psutil.boot_time()),
    }
