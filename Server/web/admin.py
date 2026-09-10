from pathlib import Path
import sys

BASE = Path(__file__).resolve().parents[1]
if str(BASE) not in sys.path:
    sys.path.insert(0, str(BASE))

from fastapi import FastAPI, Body, Query
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles

from web.services.system_monitor import get_system_status
from web.services.internet import internet_status
from web.services.clients import get_clients
from web.services.logs import tail_log
from web.services.supervisor import status as supervisor_status
from web.services.supervisor import control as supervisor_control
from services.home_assistant import home_assistant_connection

from web.console import router as console_router
from web.config_api import router as config_router
from web.ha_api import router as ha_router

app = FastAPI(title="ViPi Server Admin", docs_url=None, redoc_url=None)

WEB = Path(__file__).resolve().parent
app.mount("/static", StaticFiles(directory=WEB / "static"), name="static")

app.include_router(console_router)
app.include_router(config_router)
app.include_router(ha_router)



@app.get("/config", response_class=HTMLResponse)
async def config_page():
    return (
        WEB / "templates" / "config.html"
    ).read_text(encoding="utf-8")


@app.get("/console", response_class=HTMLResponse)
async def console_page():
    return (
        WEB / "templates" / "console.html"
    ).read_text(encoding="utf-8")


@app.get("/", response_class=HTMLResponse)
async def index():
    return (WEB / "templates" / "index.html").read_text(encoding="utf-8")

@app.get("/api/dashboard")
async def dashboard():
    return {
        "system": get_system_status(),
        "internet": await internet_status(),
        "clients": get_clients(),
        "supervisor": supervisor_status(),
        "home_assistant": await home_assistant_connection.status(),
        "version": {
            "server": server_version(),
            "client_latest": await latest_client(),
        },
    }

@app.get("/api/logs")
async def logs(service: str = Query(default="vipi-server")):
    return {"log": tail_log(service)}

@app.post("/api/control")
async def control(payload: dict = Body(...)):
    return supervisor_control(
        str(payload.get("service", "")),
        str(payload.get("action", "")),
    )


# ===== ViPi firmware/version management =====
from web.services.firmware_update import (
    server_version,
    latest_client,
    client_status,
    update_client,
    compare_versions,
)


@app.get("/api/version")
async def version_info():
    return {
        "server": {
            "version": server_version(),
        },
        "client": await latest_client(),
    }


@app.get("/api/client/status")
async def api_client_status(ip: str = Query(...)):
    latest = await latest_client()
    installed = await client_status(ip)

    result = {
        "status": installed,
        "latest": latest,
        "update_available": False,
    }

    if installed.get("ok") and latest.get("ok"):
        current = installed.get("firmware", "")
        newest = latest.get("version", "")

        result["update_available"] = compare_versions(
            current,
            newest,
        )

    return result


@app.post("/api/client/ota")
async def api_client_ota(payload: dict = Body(...)):
    ip = str(payload.get("ip", "")).strip()
    return await update_client(ip)
