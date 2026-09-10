from pathlib import Path

LOG_DIR = Path("/home/server/ViPi_V2/logs")

def tail_log(name="vipi-server", lines=120):
    allowed = {"vipi-server", "vipi-web"}
    if name not in allowed:
        name = "vipi-server"

    path = LOG_DIR / f"{name}.log"
    if not path.exists():
        return ""

    try:
        content = path.read_text(encoding="utf-8", errors="replace").splitlines()
        return "\n".join(content[-max(20, min(int(lines), 500)):])
    except Exception as e:
        return f"Không đọc được log: {e}"
