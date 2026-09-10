#!/usr/bin/env python3

from __future__ import annotations

import getpass
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / "Server"
INSTALL = ROOT / "install"
SUPERVISOR_DIR = ROOT / "supervisor"
LOG_DIR = ROOT / "logs"

HOME = Path.home()
ENV = HOME / ".env"

PYTHON = ENV / "bin" / "python"
PIP = ENV / "bin" / "pip"

USER = getpass.getuser()

SUPERVISOR_CONF = SUPERVISOR_DIR / "supervisord.conf"


def banner(text: str):
    print()
    print("=" * 60)
    print(text)
    print("=" * 60)


def run(
    cmd,
    *,
    cwd=None,
    check=True,
    capture=False,
    env=None,
):
    cmd = [str(x) for x in cmd]

    print("+", " ".join(cmd))

    kwargs = {
        "cwd": str(cwd) if cwd else None,
        "check": check,
        "text": True,
        "env": env,
    }

    if capture:
        kwargs["stdout"] = subprocess.PIPE
        kwargs["stderr"] = subprocess.STDOUT

    return subprocess.run(cmd, **kwargs)


def command(name: str):
    return shutil.which(name)


def ensure_dirs():
    SUPERVISOR_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)


def ensure_venv():
    banner(f"PYTHON ENV: {ENV}")

    if not PYTHON.exists():
        print("Tạo virtual environment mới...")
        run([sys.executable, "-m", "venv", str(ENV)])
    else:
        print("Đã có env:", ENV)

    run([
        PYTHON,
        "-m",
        "pip",
        "install",
        "--upgrade",
        "pip",
        "setuptools",
        "wheel",
    ])


def install_python_dependencies():
    banner("CÀI PYTHON DEPENDENCIES")

    req = ROOT / "requirements.txt"

    if not req.exists():
        raise RuntimeError(f"Không tìm thấy {req}")

    run([
        PYTHON,
        "-m",
        "pip",
        "install",
        "-r",
        req,
    ])


def verify_imports():
    banner("KIỂM TRA PYTHON MODULE")

    code = r"""
import importlib

mods = [
    "aiohttp",
    "dateparser",
    "edge_tts",
    "fastapi",
    "feedparser",
    "httpx",
    "numpy",
    "onnxruntime",
    "opuslib",
    "psutil",
    "yaml",
    "scipy",
    "silero_vad",
]

failed = []

for mod in mods:
    try:
        importlib.import_module(mod)
        print("PASS", mod)
    except Exception as exc:
        failed.append((mod, repr(exc)))
        print("FAIL", mod, repr(exc))

if failed:
    raise RuntimeError("Thiếu Python module")
"""

    run([PYTHON, "-c", code])


def verify_system_dependencies():
    banner("KIỂM TRA SYSTEM DEPENDENCIES")

    required = [
        "ffmpeg",
        "supervisord",
        "supervisorctl",
    ]

    optional = [
        "git",
    ]

    missing = []

    for name in required:
        path = command(name)

        if path:
            print("PASS", name, "->", path)
        else:
            print("FAIL", name)
            missing.append(name)

    for name in optional:
        path = command(name)

        if path:
            print("PASS", name, "->", path)
        else:
            print("WARN", name, "không tìm thấy")

    if missing:
        print()
        print(
            "Thiếu system package:",
            ", ".join(missing),
        )
        print(
            "Debian/Ubuntu có thể cài bằng:"
        )
        print(
            "sudo apt update && sudo apt install -y "
            "ffmpeg supervisor python3-venv"
        )
        raise RuntimeError(
            "Thiếu system dependency"
        )


def write_supervisor():
    banner("TẠO SUPERVISOR CONFIG")

    py = str(PYTHON)
    root = str(ROOT)
    server = str(SERVER)
    logs = str(LOG_DIR)
    sock = str(SUPERVISOR_DIR / "supervisor.sock")
    pid = str(SUPERVISOR_DIR / "supervisord.pid")

    env_path = (
        f"{ENV / 'bin'}:"
        "/usr/local/sbin:"
        "/usr/local/bin:"
        "/usr/sbin:"
        "/usr/bin:"
        "/sbin:"
        "/bin"
    )

    config = f"""[unix_http_server]
file={sock}
chmod=0700

[supervisord]
logfile={logs}/supervisord.log
pidfile={pid}
childlogdir={logs}
nodaemon=false

[rpcinterface:supervisor]
supervisor.rpcinterface_factory=supervisor.rpcinterface:make_main_rpcinterface

[supervisorctl]
serverurl=unix://{sock}

[program:vipi-server]
directory={server}
command={py} -u -m main
autostart=true
autorestart=true
startsecs=3
stopasgroup=true
killasgroup=true
environment=PYTHONUNBUFFERED="1",PATH="{env_path}"
stdout_logfile={logs}/vipi-server.log
stderr_logfile={logs}/vipi-server.log
stdout_logfile_maxbytes=10MB
stdout_logfile_backups=3

[program:vipi-web]
directory={server}
command={py} -u -m uvicorn web.admin:app --host 0.0.0.0 --port 8090
autostart=true
autorestart=true
startsecs=2
stopasgroup=true
killasgroup=true
environment=PYTHONUNBUFFERED="1",PATH="{env_path}"
stdout_logfile={logs}/vipi-web.log
stderr_logfile={logs}/vipi-web.log
stdout_logfile_maxbytes=5MB
stdout_logfile_backups=3
"""

    SUPERVISOR_CONF.write_text(
        config,
        encoding="utf-8",
    )

    print("PASS", SUPERVISOR_CONF)


def compile_server():
    banner("COMPILE VIPI V2")

    run([
        PYTHON,
        "-m",
        "compileall",
        "-q",
        SERVER,
    ])

    print("SERVER COMPILE: PASS")


def test_app_import():
    banner("KIỂM TRA IMPORT VIPI")

    env = os.environ.copy()
    env["PYTHONPATH"] = str(SERVER)

    run(
        [
            PYTHON,
            "-c",
            (
                "from main import build_app; "
                "app=build_app(); "
                "print('SKILLS:', len(app.registry.skills)); "
                "print('INTENTS:', len(app.registry.intent_specs))"
            ),
        ],
        cwd=SERVER,
        env=env,
    )


def shutdown_old_supervisor():
    ctl = command("supervisorctl")

    if not ctl:
        return

    if not SUPERVISOR_CONF.exists():
        return

    result = subprocess.run(
        [
            ctl,
            "-c",
            str(SUPERVISOR_CONF),
            "shutdown",
        ],
        capture_output=True,
        text=True,
    )

    out = (result.stdout + result.stderr).strip()

    if out:
        print(out)

    time.sleep(1)


def start_supervisor():
    banner("KHỞI ĐỘNG SUPERVISOR")

    supervisord = command("supervisord")
    supervisorctl = command("supervisorctl")

    if not supervisord or not supervisorctl:
        raise RuntimeError(
            "Không tìm thấy Supervisor"
        )

    # Dọn socket/pid cũ nếu daemon không còn tồn tại.
    sock = SUPERVISOR_DIR / "supervisor.sock"
    pidfile = SUPERVISOR_DIR / "supervisord.pid"

    if pidfile.exists():
        try:
            pid = int(pidfile.read_text().strip())
            os.kill(pid, 0)
        except Exception:
            pidfile.unlink(missing_ok=True)
            sock.unlink(missing_ok=True)

    result = subprocess.run(
        [
            supervisorctl,
            "-c",
            str(SUPERVISOR_CONF),
            "status",
        ],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        run([
            supervisord,
            "-c",
            SUPERVISOR_CONF,
        ])
        time.sleep(3)
    else:
        run([
            supervisorctl,
            "-c",
            SUPERVISOR_CONF,
            "reread",
        ], check=False)

        run([
            supervisorctl,
            "-c",
            SUPERVISOR_CONF,
            "update",
        ], check=False)

        run([
            supervisorctl,
            "-c",
            SUPERVISOR_CONF,
            "restart",
            "vipi-server",
            "vipi-web",
        ], check=False)

        time.sleep(3)

    status = run(
        [
            supervisorctl,
            "-c",
            SUPERVISOR_CONF,
            "status",
        ],
        check=False,
        capture=True,
    )

    print(status.stdout)

    if (
        "vipi-server" not in status.stdout
        or "vipi-web" not in status.stdout
        or "FATAL" in status.stdout
    ):
        raise RuntimeError(
            "Supervisor chưa khởi động ViPi thành công"
        )


def print_summary():
    banner("VIPI V2 SERVER INSTALL PASS")

    print("USER       :", USER)
    print("HOME       :", HOME)
    print("PROJECT    :", ROOT)
    print("ENV        :", ENV)
    print("PYTHON     :", PYTHON)
    print("SUPERVISOR :", SUPERVISOR_CONF)
    print("WEB        : http://0.0.0.0:8090")
    print()
    print(
        "Status:"
    )
    print(
        f"supervisorctl -c {SUPERVISOR_CONF} status"
    )


def main():
    print("ViPi V2 Server Installer")
    print("Project:", ROOT)

    ensure_dirs()
    verify_system_dependencies()
    ensure_venv()
    install_python_dependencies()
    verify_imports()
    compile_server()
    test_app_import()

    # Config cũ có thể đang điều khiển server hiện tại.
    # Chỉ shutdown sau khi env mới đã kiểm tra PASS.
    shutdown_old_supervisor()

    write_supervisor()
    start_supervisor()
    print_summary()


if __name__ == "__main__":
    main()
