from fastapi import APIRouter, Body, HTTPException

from config.manager import config_manager
from config.secrets import secret_manager
from services.home_assistant import home_assistant_connection


router = APIRouter()


def _public_config():
    cfg = config_manager.load().get(
        "home_assistant",
        {}
    )

    return {
        "enabled": bool(
            cfg.get("enabled", False)
        ),
        "internal_url": str(
            cfg.get("internal_url", "")
        ),
        "external_url": str(
            cfg.get("external_url", "")
        ),
        "prefer_internal": bool(
            cfg.get("prefer_internal", True)
        ),
        "timeout": int(
            cfg.get("timeout", 5)
        ),
        "token_configured":
            secret_manager.configured(
                "home_assistant",
                "token",
            ),
    }


@router.get("/api/config/home-assistant")
async def get_home_assistant_config():
    return _public_config()


@router.put("/api/config/home-assistant")
async def save_home_assistant(
    payload: dict = Body(...)
):
    internal_url = str(
        payload.get("internal_url", "")
    ).strip().rstrip("/")

    external_url = str(
        payload.get("external_url", "")
    ).strip().rstrip("/")

    enabled = bool(
        payload.get("enabled", False)
    )

    prefer_internal = bool(
        payload.get("prefer_internal", True)
    )

    try:
        timeout = int(
            payload.get("timeout", 5)
        )
    except Exception:
        raise HTTPException(
            400,
            "Timeout không hợp lệ"
        )

    timeout = max(2, min(timeout, 20))

    for label, url in (
        ("Địa chỉ nội bộ", internal_url),
        ("Địa chỉ ngoài", external_url),
    ):
        if url and not (
            url.startswith("http://")
            or url.startswith("https://")
        ):
            raise HTTPException(
                400,
                f"{label} phải bắt đầu bằng http:// hoặc https://"
            )

    config_manager.load()

    config_manager.update_section(
        "home_assistant",
        {
            "enabled": enabled,
            "internal_url": internal_url,
            "external_url": external_url,
            "prefer_internal": prefer_internal,
            "timeout": timeout,
        }
    )

    token = payload.get("token")

    if token is not None:
        token = str(token).strip()

        if token:
            secret_manager.set(
                "home_assistant",
                "token",
                token,
            )

    if bool(payload.get("clear_token", False)):
        secret_manager.set(
            "home_assistant",
            "token",
            "",
        )

    home_assistant_connection.clear_cache()

    return {
        "ok": True,
        "config": _public_config(),
    }


@router.post("/api/home-assistant/test")
async def test_home_assistant(
    payload: dict = Body(default={})
):
    mode = str(
        payload.get("mode", "auto")
    ).strip().lower()

    if mode not in (
        "auto",
        "internal",
        "external",
    ):
        raise HTTPException(
            400,
            "Mode test không hợp lệ"
        )

    internal_url = str(
        payload.get("internal_url", "")
    ).strip()

    external_url = str(
        payload.get("external_url", "")
    ).strip()

    token = str(
        payload.get("token", "")
    ).strip()

    if not token:
        token = secret_manager.get(
            "home_assistant",
            "token",
            "",
        )

    try:
        timeout = int(
            payload.get("timeout", 5)
        )
    except Exception:
        timeout = 5

    timeout = max(2, min(timeout, 20))

    async def run_one(source, url):
        item = await home_assistant_connection.test_url(
            url,
            token,
            timeout,
        )

        return {
            "ok": bool(item.get("ok")),
            "source": source,
            "url": item.get("url"),
            "latency_ms": item.get("latency_ms"),
            "version": item.get("version"),
            "message": item.get(
                "message",
                "Không kết nối được",
            ),
        }

    if mode == "internal":
        return await run_one(
            "internal",
            internal_url,
        )

    if mode == "external":
        return await run_one(
            "external",
            external_url,
        )

    prefer_internal = bool(
        payload.get(
            "prefer_internal",
            True,
        )
    )

    if prefer_internal:
        targets = (
            ("internal", internal_url),
            ("external", external_url),
        )
    else:
        targets = (
            ("external", external_url),
            ("internal", internal_url),
        )

    attempts = []

    for source, url in targets:
        if not url:
            continue

        item = await run_one(
            source,
            url,
        )

        attempts.append(item)

        if item["ok"]:
            return {
                **item,
                "attempts": attempts,
            }

    if attempts:
        last = attempts[-1]

        return {
            **last,
            "attempts": attempts,
        }

    return {
        "ok": False,
        "source": None,
        "url": None,
        "latency_ms": None,
        "version": None,
        "message": "Chưa nhập địa chỉ Home Assistant",
        "attempts": [],
    }



@router.get("/api/home-assistant/discovery")
async def home_assistant_discovery(
    refresh: bool = False
):
    from services.home_assistant import (
        home_assistant_registry,
    )

    try:
        data = await home_assistant_registry.discover(
            force=refresh
        )

        return {
            "ok": True,
            **data,
        }

    except Exception as e:
        return {
            "ok": False,
            "message": str(e),
            "areas": [],
            "entities": [],
            "unassigned": [],
            "counts": {
                "areas": 0,
                "devices": 0,
                "entities": 0,
                "unassigned": 0,
            },
        }


@router.post("/api/home-assistant/service")
async def home_assistant_service(
    payload: dict = Body(...)
):
    from services.home_assistant import (
        home_assistant_registry,
    )

    domain = str(
        payload.get("domain", "")
    ).strip()

    service = str(
        payload.get("service", "")
    ).strip()

    entity_ids = payload.get(
        "entity_ids",
        []
    )

    # Web API intentionally limited.
    if domain not in (
        "light",
        "switch",
        "fan",
    ):
        raise HTTPException(
            400,
            "Domain không được phép"
        )

    if service not in (
        "turn_on",
        "turn_off",
    ):
        raise HTTPException(
            400,
            "Service không được phép"
        )

    if not isinstance(
        entity_ids,
        list,
    ):
        raise HTTPException(
            400,
            "entity_ids không hợp lệ"
        )

    try:
        await home_assistant_registry.call_service(
            domain,
            service,
            entity_ids,
        )

        return {
            "ok": True,
            "entities": entity_ids,
        }

    except Exception as e:
        raise HTTPException(
            502,
            str(e),
        )
