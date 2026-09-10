import asyncio
import shutil
from pathlib import Path

from fastapi import APIRouter, Body, HTTPException

router = APIRouter()

SUPERVISOR_CONF = "/home/server/ViPi_V2/supervisor/supervisord.conf"
SERVER_ROOT = Path("/home/server/ViPi_V2/Server")


async def _run(argv, timeout=8):
    try:
        proc = await asyncio.create_subprocess_exec(
            *argv,
            cwd=str(SERVER_ROOT),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )

        out, _ = await asyncio.wait_for(
            proc.communicate(),
            timeout=timeout
        )

        text = out.decode("utf-8", errors="replace").strip()

        return {
            "ok": proc.returncode == 0,
            "code": proc.returncode,
            "output": text or "(không có output)",
        }

    except asyncio.TimeoutError:
        try:
            proc.kill()
        except Exception:
            pass

        return {
            "ok": False,
            "code": -1,
            "output": "Lệnh quá thời gian cho phép.",
        }

    except Exception as e:
        return {
            "ok": False,
            "code": -1,
            "output": f"Lỗi: {e}",
        }


async def _supervisor(action, service=None):
    supervisorctl = shutil.which("supervisorctl") or "/usr/bin/supervisorctl"

    cmd = [
        supervisorctl,
        "-c",
        SUPERVISOR_CONF,
    ]

    if action == "status":
        cmd.append("status")
    else:
        cmd.extend([action, service])

    return await _run(cmd)


COMMANDS = {
    "vipi status": (
        "Supervisor status",
        lambda: _supervisor("status"),
    ),

    "vipi ports": (
        "Listening ports",
        lambda: _run([
            "/usr/bin/ss",
            "-lntup",
        ]),
    ),

    "vipi memory": (
        "Memory",
        lambda: _run([
            "/usr/bin/free",
            "-h",
        ]),
    ),

    "vipi disk": (
        "Disk",
        lambda: _run([
            "/usr/bin/df",
            "-h",
            "/",
        ]),
    ),

    "vipi uptime": (
        "Uptime",
        lambda: _run([
            "/usr/bin/uptime",
        ]),
    ),

    "vipi ip": (
        "IP addresses",
        lambda: _run([
            "/usr/bin/hostname",
            "-I",
        ]),
    ),

    "vipi restart": (
        "Restart ViPi Server",
        lambda: _supervisor("restart", "vipi-server"),
    ),

    "vipi start": (
        "Start ViPi Server",
        lambda: _supervisor("start", "vipi-server"),
    ),

    "vipi stop": (
        "Stop ViPi Server",
        lambda: _supervisor("stop", "vipi-server"),
    ),

    "web restart": (
        "Restart Web Admin",
        lambda: _supervisor("restart", "vipi-web"),
    ),
}


@router.get("/api/console/commands")
async def commands():
    return {
        "commands": [
            {
                "command": command,
                "description": description,
            }
            for command, (description, _) in COMMANDS.items()
        ]
    }


@router.post("/api/console/run")
async def run_console(payload: dict = Body(...)):
    command = str(payload.get("command", "")).strip().lower()

    if not command:
        raise HTTPException(400, "Thiếu lệnh")

    if command == "clear":
        return {
            "ok": True,
            "code": 0,
            "output": "",
            "clear": True,
        }

    if command == "help":
        lines = ["ViPi Web Console", ""]

        for cmd, (desc, _) in COMMANDS.items():
            lines.append(f"{cmd:<18} {desc}")

        lines.extend([
            "",
            "help               Danh sách lệnh",
            "clear              Xóa màn hình",
        ])

        return {
            "ok": True,
            "code": 0,
            "output": "\n".join(lines),
        }

    item = COMMANDS.get(command)

    if item is None:
        raise HTTPException(
            403,
            "Lệnh không được phép. Gõ help để xem danh sách."
        )

    _, runner = item
    return await runner()
