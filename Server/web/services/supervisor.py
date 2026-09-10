import subprocess

CTL = "/usr/bin/supervisorctl"
CONF = "/home/server/ViPi_V2/supervisor/supervisord.conf"

ALLOWED = {"vipi-server", "vipi-web"}
ACTIONS = {"start", "stop", "restart"}

def _run(*args):
    try:
        p = subprocess.run(
            [CTL, "-c", CONF, *args],
            capture_output=True,
            text=True,
            timeout=8,
        )
        return {
            "ok": p.returncode == 0,
            "output": (p.stdout + p.stderr).strip()
        }
    except Exception as e:
        return {"ok": False, "output": str(e)}

def status():
    return _run("status")

def control(service, action):
    if service not in ALLOWED or action not in ACTIONS:
        return {"ok": False, "output": "Lệnh không hợp lệ"}
    return _run(action, service)
