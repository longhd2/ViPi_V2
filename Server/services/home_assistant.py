from __future__ import annotations

import time
from urllib.parse import urljoin

import httpx

from config.manager import config_manager
from config.secrets import secret_manager


class HomeAssistantConnection:
    def __init__(self):
        self._cache = {
            "time": 0.0,
            "result": {
                "configured": False,
                "connected": False,
                "source": None,
                "url": None,
                "latency_ms": None,
                "version": None,
                "message": "Chưa cấu hình",
            },
        }

    def config(self):
        cfg = config_manager.load().get(
            "home_assistant",
            {}
        )

        token = secret_manager.get(
            "home_assistant",
            "token",
            "",
        )

        return cfg, token

    @staticmethod
    def _clean_url(url):
        return str(url or "").strip().rstrip("/")

    async def test_url(self, url, token, timeout=5):
        url = self._clean_url(url)

        if not url:
            return {
                "ok": False,
                "url": "",
                "latency_ms": None,
                "version": None,
                "message": "Chưa nhập địa chỉ",
            }

        if not token:
            return {
                "ok": False,
                "url": url,
                "latency_ms": None,
                "version": None,
                "message": "Chưa cấu hình Access Token",
            }

        started = time.monotonic()

        headers = {
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
        }

        try:
            async with httpx.AsyncClient(
                timeout=float(timeout),
                follow_redirects=True,
            ) as client:

                r = await client.get(
                    url + "/api/",
                    headers=headers,
                )

            latency = round(
                (time.monotonic() - started) * 1000
            )

            if r.status_code == 200:
                data = {}

                try:
                    data = r.json()
                except Exception:
                    pass

                return {
                    "ok": True,
                    "url": url,
                    "latency_ms": latency,
                    "version": data.get("version"),
                    "message": "Kết nối thành công",
                }

            if r.status_code in (401, 403):
                message = "Token không hợp lệ hoặc không có quyền"
            else:
                message = f"HTTP {r.status_code}"

            return {
                "ok": False,
                "url": url,
                "latency_ms": latency,
                "version": None,
                "message": message,
            }

        except httpx.ConnectTimeout:
            message = "Hết thời gian kết nối"

        except httpx.ReadTimeout:
            message = "Home Assistant phản hồi quá chậm"

        except httpx.ConnectError:
            message = "Không kết nối được địa chỉ này"

        except Exception as e:
            message = str(e)

        return {
            "ok": False,
            "url": url,
            "latency_ms": None,
            "version": None,
            "message": message,
        }

    async def test(self, mode="auto"):
        cfg, token = self.config()

        internal = self._clean_url(
            cfg.get("internal_url")
        )

        external = self._clean_url(
            cfg.get("external_url")
        )

        timeout = cfg.get("timeout", 5)

        if mode == "internal":
            result = await self.test_url(
                internal,
                token,
                timeout,
            )

            result["source"] = "internal"
            return result

        if mode == "external":
            result = await self.test_url(
                external,
                token,
                timeout,
            )

            result["source"] = "external"
            return result

        prefer_internal = bool(
            cfg.get("prefer_internal", True)
        )

        targets = (
            [
                ("internal", internal),
                ("external", external),
            ]
            if prefer_internal
            else
            [
                ("external", external),
                ("internal", internal),
            ]
        )

        attempts = []

        for source, url in targets:
            if not url:
                continue

            result = await self.test_url(
                url,
                token,
                timeout,
            )

            result["source"] = source
            attempts.append(result)

            if result["ok"]:
                result["attempts"] = attempts
                return result

        if attempts:
            result = dict(attempts[-1])
            result["attempts"] = attempts
            return result

        return {
            "ok": False,
            "source": None,
            "url": None,
            "latency_ms": None,
            "version": None,
            "message": "Chưa nhập địa chỉ Home Assistant",
            "attempts": [],
        }

    async def status(self, max_age=10):
        now = time.monotonic()

        if now - self._cache["time"] < max_age:
            return dict(self._cache["result"])

        cfg, token = self.config()

        configured = bool(
            token
            and (
                self._clean_url(cfg.get("internal_url"))
                or
                self._clean_url(cfg.get("external_url"))
            )
        )

        if not configured:
            result = {
                "configured": False,
                "connected": False,
                "source": None,
                "url": None,
                "latency_ms": None,
                "version": None,
                "message": "Chưa cấu hình",
            }

        elif not cfg.get("enabled", False):
            result = {
                "configured": True,
                "connected": False,
                "source": None,
                "url": None,
                "latency_ms": None,
                "version": None,
                "message": "Đã cấu hình · đang tắt",
            }

        else:
            test = await self.test("auto")

            result = {
                "configured": True,
                "connected": bool(test.get("ok")),
                "source": test.get("source"),
                "url": test.get("url"),
                "latency_ms": test.get("latency_ms"),
                "version": test.get("version"),
                "message": test.get("message"),
            }

        self._cache = {
            "time": now,
            "result": result,
        }

        return dict(result)

    def clear_cache(self):
        self._cache["time"] = 0.0


home_assistant_connection = HomeAssistantConnection()


# ============================================================
# HA REGISTRY / DISCOVERY / SERVICE CONTROL
# ============================================================

import asyncio
import unicodedata

import aiohttp


def _ha_normalize(value):
    value = str(value or "").strip().lower()

    value = unicodedata.normalize(
        "NFD",
        value,
    )

    value = "".join(
        c for c in value
        if unicodedata.category(c) != "Mn"
    )

    return " ".join(value.split())


class HomeAssistantRegistry:

    def __init__(self, connection):
        self.connection = connection

        self._cache = {
            "time": 0.0,
            "data": None,
        }

        self._lock = asyncio.Lock()

    async def _active_connection(self):
        cfg, token = self.connection.config()

        test = await self.connection.test(
            "auto"
        )

        if not test.get("ok"):
            raise RuntimeError(
                test.get(
                    "message",
                    "Home Assistant không kết nối được",
                )
            )

        return (
            test.get("url"),
            token,
            cfg,
        )

    @staticmethod
    def _ws_url(base_url):
        base_url = str(
            base_url
        ).rstrip("/")

        if base_url.startswith("https://"):
            return (
                "wss://"
                + base_url[len("https://"):]
                + "/api/websocket"
            )

        if base_url.startswith("http://"):
            return (
                "ws://"
                + base_url[len("http://"):]
                + "/api/websocket"
            )

        raise RuntimeError(
            "Home Assistant URL không hợp lệ"
        )

    async def _ws_command(
        self,
        ws,
        command_id,
        command_type,
    ):
        await ws.send_json({
            "id": command_id,
            "type": command_type,
        })

        while True:
            msg = await ws.receive(
                timeout=10
            )

            if msg.type != aiohttp.WSMsgType.TEXT:
                raise RuntimeError(
                    "Home Assistant WebSocket bị ngắt"
                )

            data = msg.json()

            if (
                data.get("type") == "result"
                and data.get("id") == command_id
            ):
                if not data.get("success"):
                    error = data.get("error") or {}

                    raise RuntimeError(
                        error.get(
                            "message",
                            f"Lỗi {command_type}",
                        )
                    )

                return data.get("result") or []

    async def _fetch_registry(self):
        base_url, token, cfg = (
            await self._active_connection()
        )

        ws_url = self._ws_url(
            base_url
        )

        timeout = aiohttp.ClientTimeout(
            total=max(
                10,
                int(cfg.get("timeout", 5)) * 2,
            )
        )

        async with aiohttp.ClientSession(
            timeout=timeout
        ) as session:

            async with session.ws_connect(
                ws_url,
                heartbeat=20,
            ) as ws:

                hello = await ws.receive_json(
                    timeout=10
                )

                if (
                    hello.get("type")
                    != "auth_required"
                ):
                    raise RuntimeError(
                        "HA WebSocket không yêu cầu xác thực"
                    )

                await ws.send_json({
                    "type": "auth",
                    "access_token": token,
                })

                auth = await ws.receive_json(
                    timeout=10
                )

                if auth.get("type") != "auth_ok":
                    raise RuntimeError(
                        "HA WebSocket xác thực thất bại"
                    )

                areas = await self._ws_command(
                    ws,
                    1,
                    "config/area_registry/list",
                )

                devices = await self._ws_command(
                    ws,
                    2,
                    "config/device_registry/list",
                )

                entities = await self._ws_command(
                    ws,
                    3,
                    "config/entity_registry/list",
                )

        states = await self.states(
            base_url=base_url,
            token=token,
        )

        return self._build(
            base_url,
            areas,
            devices,
            entities,
            states,
        )

    async def states(
        self,
        base_url=None,
        token=None,
    ):
        if not base_url or not token:
            base_url, token, cfg = (
                await self._active_connection()
            )
        else:
            cfg, _ = self.connection.config()

        headers = {
            "Authorization":
                f"Bearer {token}",
        }

        timeout = float(
            cfg.get("timeout", 5)
        )

        async with aiohttp.ClientSession() as session:
            async with session.get(
                base_url.rstrip("/")
                + "/api/states",
                headers=headers,
                timeout=timeout,
            ) as response:

                if response.status != 200:
                    raise RuntimeError(
                        f"HA states HTTP {response.status}"
                    )

                return await response.json()

    def _build(
        self,
        base_url,
        areas,
        devices,
        entities,
        states,
    ):
        area_map = {
            a.get("area_id"): {
                "area_id": a.get("area_id"),
                "name": a.get("name") or a.get("area_id"),
                "aliases": a.get("aliases") or [],
                "entities": [],
            }
            for a in areas
            if a.get("area_id")
        }

        device_map = {
            d.get("id"): d
            for d in devices
            if d.get("id")
        }

        entity_registry = {
            e.get("entity_id"): e
            for e in entities
            if e.get("entity_id")
        }

        state_map = {
            s.get("entity_id"): s
            for s in states
            if s.get("entity_id")
        }

        discovered = []

        for entity_id, state in state_map.items():

            if "." not in entity_id:
                continue

            domain = entity_id.split(
                ".",
                1,
            )[0]

            if domain not in (
                "light",
                "switch",
                "fan",
                "climate",
            ):
                continue

            reg = entity_registry.get(
                entity_id,
                {}
            )

            device_id = reg.get(
                "device_id"
            )

            area_id = reg.get(
                "area_id"
            )

            if not area_id and device_id:
                device = device_map.get(
                    device_id,
                    {}
                )

                area_id = device.get(
                    "area_id"
                )

            attributes = (
                state.get("attributes")
                or {}
            )

            name = (
                reg.get("name")
                or reg.get("original_name")
                or attributes.get("friendly_name")
                or entity_id
            )

            aliases = list(
                reg.get("aliases")
                or []
            )

            item = {
                "entity_id": entity_id,
                "domain": domain,
                "name": name,
                "aliases": aliases,
                "state": state.get("state"),
                "area_id": area_id,
                "device_id": device_id,
                "disabled": bool(
                    reg.get("disabled_by")
                ),
            }

            discovered.append(item)

            if (
                area_id
                and area_id in area_map
            ):
                area_map[
                    area_id
                ]["entities"].append(
                    item
                )

        unassigned = [
            e for e in discovered
            if not e.get("area_id")
        ]

        result = {
            "connected_url": base_url,
            "areas": sorted(
                area_map.values(),
                key=lambda x:
                    _ha_normalize(
                        x.get("name")
                    ),
            ),
            "entities": sorted(
                discovered,
                key=lambda x:
                    _ha_normalize(
                        x.get("name")
                    ),
            ),
            "unassigned": sorted(
                unassigned,
                key=lambda x:
                    _ha_normalize(
                        x.get("name")
                    ),
            ),
            "counts": {
                "areas": len(area_map),
                "devices": len(device_map),
                "entities": len(discovered),
                "unassigned": len(unassigned),
            },
        }

        return result

    async def discover(
        self,
        force=False,
        max_age=30,
    ):
        now = time.monotonic()

        if (
            not force
            and self._cache["data"] is not None
            and now - self._cache["time"] < max_age
        ):
            return self._cache["data"]

        async with self._lock:

            now = time.monotonic()

            if (
                not force
                and self._cache["data"] is not None
                and now - self._cache["time"] < max_age
            ):
                return self._cache["data"]

            data = await self._fetch_registry()

            self._cache = {
                "time": time.monotonic(),
                "data": data,
            }

            return data

    def clear_cache(self):
        self._cache = {
            "time": 0.0,
            "data": None,
        }

    async def call_service(
        self,
        domain,
        service,
        entity_ids,
    ):
        base_url, token, cfg = (
            await self._active_connection()
        )

        if isinstance(entity_ids, str):
            entity_ids = [entity_ids]

        entity_ids = [
            str(x).strip()
            for x in entity_ids
            if str(x).strip()
        ]

        if not entity_ids:
            raise RuntimeError(
                "Không có entity để điều khiển"
            )

        headers = {
            "Authorization":
                f"Bearer {token}",
            "Content-Type":
                "application/json",
        }

        url = (
            base_url.rstrip("/")
            + f"/api/services/{domain}/{service}"
        )

        payload = {
            "entity_id": entity_ids,
        }

        timeout = float(
            cfg.get("timeout", 5)
        )

        async with aiohttp.ClientSession() as session:
            async with session.post(
                url,
                headers=headers,
                json=payload,
                timeout=timeout,
            ) as response:

                if response.status not in (
                    200,
                    201,
                ):
                    text = await response.text()

                    raise RuntimeError(
                        f"HA service HTTP "
                        f"{response.status}: "
                        f"{text[:120]}"
                    )

                try:
                    data = await response.json()
                except Exception:
                    data = []

        self.clear_cache()

        return data

    async def resolve_lights(
        self,
        text,
        room=None,
    ):
        data = await self.discover(
            force=True
        )

        query = _ha_normalize(
            text
        )

        room_query = _ha_normalize(
            room
        )

        lights = [
            e for e in data["entities"]
            if (
                e["domain"] == "light"
                and not e["disabled"]
            )
        ]

        # 1. Exact/specific entity name or alias spoken.
        candidates = []

        for entity in lights:
            names = [
                entity["name"],
                *entity.get(
                    "aliases",
                    []
                ),
            ]

            names_norm = [
                _ha_normalize(x)
                for x in names
                if x
            ]

            best = max(
                (
                    len(name)
                    for name in names_norm
                    if name
                    and name in query
                ),
                default=0,
            )

            if best:
                candidates.append(
                    (
                        best,
                        entity,
                    )
                )

        if candidates:
            max_score = max(
                x[0]
                for x in candidates
            )

            selected = [
                x[1]
                for x in candidates
                if x[0] == max_score
            ]

            return {
                "kind": "entity",
                "entities": selected,
                "area": None,
            }

        # 2. Explicit spoken HA area.
        area_matches = []

        for area in data["areas"]:
            names = [
                area["name"],
                *area.get(
                    "aliases",
                    []
                ),
            ]

            for name in names:
                normalized = (
                    _ha_normalize(name)
                )

                if (
                    normalized
                    and normalized in query
                ):
                    area_matches.append(
                        (
                            len(normalized),
                            area,
                        )
                    )

        if area_matches:
            area = max(
                area_matches,
                key=lambda x: x[0],
            )[1]

            selected = [
                e
                for e in area["entities"]
                if (
                    e["domain"] == "light"
                    and not e["disabled"]
                )
            ]

            return {
                "kind": "area",
                "entities": selected,
                "area": area,
            }

        # 3. Device context room.
        if room_query and room_query != "default":
            for area in data["areas"]:
                names = [
                    area["name"],
                    *area.get(
                        "aliases",
                        []
                    ),
                ]

                if any(
                    _ha_normalize(name)
                    == room_query
                    for name in names
                ):
                    selected = [
                        e
                        for e in area["entities"]
                        if (
                            e["domain"] == "light"
                            and not e["disabled"]
                        )
                    ]

                    return {
                        "kind": "context",
                        "entities": selected,
                        "area": area,
                    }

        return {
            "kind": "ambiguous",
            "entities": [],
            "area": None,
        }


home_assistant_registry = HomeAssistantRegistry(
    home_assistant_connection
)
