from __future__ import annotations

import re
import time
import unicodedata
from difflib import SequenceMatcher

from .base import Skill, SkillResult
from services.home_assistant import (
    home_assistant_connection,
    home_assistant_registry,
)


# ============================================================
# ViPi HASS Skill V2
# - No hardcoded entity_id
# - NAME first
# - Exact -> partial -> fuzzy
# - Multi target / "và"
# - Area / all
# - State guard
# - State query / area status
# - Per-device pending dialogue
# ============================================================

CONTROL_DOMAINS = {
    "light",
    "switch",
    "fan",
    "climate",
    "cover",
    "script",
    "scene",
    "automation",
}

STATUS_DOMAINS = CONTROL_DOMAINS | {
    "sensor",
    "binary_sensor",
}

DOMAIN_WORDS = {
    "light": (
        "đèn",
        "den",
    ),
    "switch": (
        "ổ cắm",
        "o cam",
        "công tắc",
        "cong tac",
        "máy hút mùi",
        "may hut mui",
    ),
    "fan": (
        "quạt",
        "quat",
    ),
    "climate": (
        "điều hòa",
        "dieu hoa",
        "máy lạnh",
        "may lanh",
    ),
    "cover": (
        "rèm",
        "rem",
        "rèm cửa",
        "rem cua",
        "cửa cuốn",
        "cua cuon",
    ),
    "script": (
        "chế độ",
        "che do",
        "kịch bản",
        "kich ban",
    ),
    "scene": (
        "cảnh",
        "canh",
        "chế độ",
        "che do",
    ),
    "automation": (
        "tự động",
        "tu dong",
        "automation",
    ),
}

ON_STATES = {
    "on",
    "open",
    "opening",
    "cool",
    "heat",
    "heat_cool",
    "fan_only",
    "dry",
}

OFF_STATES = {
    "off",
    "closed",
    "closing",
    "idle",
}

CANCEL_WORDS = {
    "thôi",
    "thoi",
    "hủy",
    "huy",
    "không cần",
    "khong can",
    "bỏ đi",
    "bo di",
}

ALL_WORDS = (
    "tất cả",
    "tat ca",
    "toàn bộ",
    "toan bo",
    "hết",
    "het",
)

ROOM_WORD = re.compile(
    r"\b(phòng|phong)\s+[\wÀ-ỹ]+(?:\s+[\wÀ-ỹ]+)?",
    re.IGNORECASE,
)

PENDING_TTL = 10.0


def norm(value):
    s = str(value or "").strip().lower()
    s = s.replace("đ", "d")

    s = unicodedata.normalize("NFD", s)
    s = "".join(
        c for c in s
        if unicodedata.category(c) != "Mn"
    )

    s = re.sub(r"[^\w\s]", " ", s)
    return " ".join(s.split())


def fuzzy(a, b):
    a = norm(a)
    b = norm(b)

    if not a or not b:
        return 0.0

    if a == b:
        return 1.0

    return SequenceMatcher(None, a, b).ratio()


def clean_target(text):
    s = norm(text)

    prefixes = (
        "bat ",
        "tat ",
        "mo ",
        "dong ",
        "chay ",
        "kich hoat ",
        "kiem tra ",
        "trang thai ",
        "xem ",
        "cho toi biet ",
    )

    changed = True
    while changed:
        changed = False
        for p in prefixes:
            if s.startswith(p):
                s = s[len(p):].strip()
                changed = True

    for word in ALL_WORDS:
        nw = norm(word)
        s = re.sub(
            rf"\b{re.escape(nw)}\b",
            " ",
            s,
        )

    return " ".join(s.split())


def human_state(entity):
    state = str(entity.get("state") or "").lower()

    if state in ("on", "open", "opening"):
        return "đang bật" if state == "on" else "đang mở"

    if state in ("off", "closed", "closing"):
        return "đang tắt" if state == "off" else "đang đóng"

    if state == "cool":
        return "đang bật chế độ làm lạnh"

    if state == "heat":
        return "đang bật chế độ sưởi"

    if state == "unavailable":
        return "hiện không khả dụng"

    if state == "unknown":
        return "chưa xác định được trạng thái"

    return f"đang ở trạng thái {state}"


class HomeAssistantSkill(Skill):
    name = "home_assistant"
    priority = 35

    def __init__(self):
        self._pending = {}

    async def match(self, intent, ctx):
        return intent.name.startswith("home.")

    # --------------------------------------------------------
    # Pending dialogue
    # --------------------------------------------------------

    def _device_id(self, ctx):
        return str(
            getattr(ctx, "device_id", "default")
            or "default"
        )

    def _set_pending(self, ctx, data):
        self._pending[self._device_id(ctx)] = {
            **data,
            "expires": time.monotonic() + PENDING_TTL,
        }

    def _get_pending(self, ctx):
        key = self._device_id(ctx)
        p = self._pending.get(key)

        if not p:
            return None

        if time.monotonic() > p.get("expires", 0):
            self._pending.pop(key, None)
            return None

        return p

    def _clear_pending(self, ctx):
        self._pending.pop(
            self._device_id(ctx),
            None,
        )

    # --------------------------------------------------------
    # HA discovery helpers
    # --------------------------------------------------------

    async def _data(self, refresh=True):
        data = await home_assistant_registry.discover(
            force=refresh,
            max_age=2,
        )

        return data or {}

    def _entities(self, data, status=False):
        domains = (
            STATUS_DOMAINS
            if status
            else CONTROL_DOMAINS
        )

        return [
            e for e in data.get("entities", [])
            if e.get("domain") in domains
            and not e.get("disabled")
        ]

    def _areas(self, data):
        return data.get("areas", []) or []

    def _area_name(self, entity, data):
        """
        Resolve area with this priority:
        1. HA area_id
        2. Area name appearing inside entity NAME/aliases

        NAME is important because MQTT/YAML entities may not be
        explicitly assigned to a HA Area.
        """
        area_id = entity.get("area_id")

        if area_id:
            for area in self._areas(data):
                if area.get("area_id") == area_id:
                    return area.get("name")

        names = [
            entity.get("name") or "",
            *(entity.get("aliases") or []),
        ]

        best = None
        best_len = 0

        for area in self._areas(data):
            area_candidates = [
                area.get("name") or "",
                *(area.get("aliases") or []),
            ]

            for ac in area_candidates:
                an = norm(ac)

                if not an:
                    continue

                for name in names:
                    nn = norm(name)

                    if an in nn and len(an) > best_len:
                        best = area.get("name")
                        best_len = len(an)

        return best

    def _find_areas_in_text(self, text, data):
        nt = norm(text)
        found = []

        for area in self._areas(data):
            name = area.get("name") or ""
            aliases = area.get("aliases") or []

            candidates = [name] + aliases

            if any(
                norm(x) and norm(x) in nt
                for x in candidates
            ):
                found.append(area)

        return found

    def _domain_hint(self, text):
        nt = norm(text)

        hits = []

        for domain, words in DOMAIN_WORDS.items():
            for word in words:
                if norm(word) in nt:
                    hits.append(domain)
                    break

        # "điện phòng..." is intentionally broad.
        if "dien" in nt and not hits:
            return None

        if len(hits) == 1:
            return hits[0]

        return None

    def _all_requested(self, text):
        nt = norm(text)

        return any(
            norm(word) in nt
            for word in ALL_WORDS
        )

    # --------------------------------------------------------
    # NAME resolver
    # --------------------------------------------------------

    def _score_entity(
        self,
        target,
        entity,
        data,
        domain_hint=None,
        areas=None,
    ):
        name = entity.get("name") or ""
        aliases = entity.get("aliases") or []

        names = [name] + list(aliases)
        target_n = clean_target(target)

        if not target_n:
            return 0.0

        best = 0.0

        for candidate in names:
            cn = norm(candidate)

            if not cn:
                continue

            if target_n == cn:
                score = 1.0

            elif target_n in cn:
                # Short spoken form:
                # "đèn bàn" -> "Đèn bàn phòng khách"
                coverage = len(target_n) / max(
                    len(cn),
                    1,
                )
                score = 0.88 + min(
                    coverage * 0.08,
                    0.08,
                )

            elif cn in target_n:
                score = 0.93

            else:
                score = fuzzy(
                    target_n,
                    cn,
                )

            best = max(best, score)

        if (
            domain_hint
            and entity.get("domain") == domain_hint
        ):
            best += 0.06

        if areas:
            area_names = {
                norm(a.get("name"))
                for a in areas
            }

            entity_area = norm(
                self._area_name(entity, data)
            )

            if entity_area in area_names:
                best += 0.08
            else:
                best -= 0.12

        return min(best, 1.0)

    def _resolve_one(
        self,
        target,
        data,
        *,
        domain_hint=None,
        areas=None,
        status=False,
    ):
        entities = self._entities(
            data,
            status=status,
        )

        if domain_hint:
            entities = [
                e for e in entities
                if e.get("domain") == domain_hint
            ]

        if areas:
            wanted = {
                norm(a.get("name"))
                for a in areas
            }

            scoped = [
                e for e in entities
                if norm(
                    self._area_name(e, data)
                ) in wanted
            ]

            if scoped:
                entities = scoped

        ranked = sorted(
            (
                (
                    self._score_entity(
                        target,
                        e,
                        data,
                        domain_hint,
                        areas,
                    ),
                    e,
                )
                for e in entities
            ),
            key=lambda x: x[0],
            reverse=True,
        )

        if not ranked:
            return {
                "kind": "none",
                "entities": [],
            }

        best_score, best = ranked[0]

        if best_score < 0.64:
            return {
                "kind": "none",
                "entities": [],
                "score": best_score,
            }

        # Ambiguous if two candidates are almost equal.
        if len(ranked) > 1:
            second_score, second = ranked[1]

            if (
                best_score < 0.97
                and second_score >= 0.68
                and (best_score - second_score) < 0.055
            ):
                return {
                    "kind": "ambiguous",
                    "entities": [
                        best,
                        second,
                    ],
                    "score": best_score,
                }

        return {
            "kind": "entity",
            "entities": [best],
            "score": best_score,
        }

    def _entities_in_areas(
        self,
        data,
        areas,
        domain_hint=None,
        status=False,
    ):
        wanted = {
            norm(a.get("name"))
            for a in areas
        }

        result = []

        for e in self._entities(
            data,
            status=status,
        ):
            if (
                domain_hint
                and e.get("domain") != domain_hint
            ):
                continue

            area = norm(
                self._area_name(e, data)
            )

            if area in wanted:
                result.append(e)

        return result

    def _split_targets(self, text):
        nt = clean_target(text)

        # Preserve phrases such as "phòng khách và phòng ngủ".
        parts = re.split(
            r"\s+(?:va|voi)\s+",
            nt,
        )

        return [
            p.strip()
            for p in parts
            if p.strip()
        ]

    def _resolve_targets(
        self,
        text,
        data,
        *,
        status=False,
    ):
        domain_hint = self._domain_hint(text)
        areas = self._find_areas_in_text(text, data)
        all_requested = self._all_requested(text)

        # ----------------------------------------------------
        # ALL
        # "tắt tất cả đèn"
        # "tắt hết đèn phòng ngủ"
        # ----------------------------------------------------
        if all_requested:
            if areas:
                entities = self._entities_in_areas(
                    data,
                    areas,
                    domain_hint,
                    status=status,
                )
            else:
                entities = self._entities(
                    data,
                    status=status,
                )

                if domain_hint:
                    entities = [
                        e for e in entities
                        if e.get("domain") == domain_hint
                    ]

            return {
                "kind": "all",
                "entities": entities,
                "domain": domain_hint,
                "areas": areas,
            }

        # ----------------------------------------------------
        # MULTI TARGET
        # "bật đèn ngủ và đèn đầu giường"
        #
        # But:
        # "tắt đèn phòng ngủ và phòng khách"
        # is an Area group, not two entity names.
        # ----------------------------------------------------
        parts = self._split_targets(text)

        if len(parts) > 1:
            # If there are multiple recognized Areas and a domain,
            # interpret category + rooms as a group command.
            if len(areas) >= 2 and domain_hint:
                entities = self._entities_in_areas(
                    data,
                    areas,
                    domain_hint,
                    status=status,
                )

                if entities:
                    return {
                        "kind": "area_group",
                        "entities": entities,
                        "domain": domain_hint,
                        "areas": areas,
                    }

            resolved_entities = []
            ambiguous = []

            for part in parts:
                part_domain = (
                    self._domain_hint(part)
                    or domain_hint
                )

                part_areas = self._find_areas_in_text(
                    part,
                    data,
                )

                r = self._resolve_one(
                    part,
                    data,
                    domain_hint=part_domain,
                    areas=part_areas,
                    status=status,
                )

                if r.get("kind") == "entity":
                    resolved_entities.extend(
                        r.get("entities", [])
                    )

                elif r.get("kind") == "ambiguous":
                    ambiguous.extend(
                        r.get("entities", [])
                    )

            unique = {
                e["entity_id"]: e
                for e in resolved_entities
            }

            if unique and not ambiguous:
                return {
                    "kind": "multi",
                    "entities": list(unique.values()),
                    "domain": domain_hint,
                    "areas": areas,
                }

            if ambiguous:
                unique_amb = {
                    e["entity_id"]: e
                    for e in ambiguous
                }

                return {
                    "kind": "ambiguous",
                    "entities": list(
                        unique_amb.values()
                    ),
                    "domain": domain_hint,
                    "areas": areas,
                }

        # ----------------------------------------------------
        # SPECIFIC NAME FIRST
        #
        # Critical:
        # "mở rèm cửa phòng khách"
        # must resolve "Rèm cửa phòng khách" before considering
        # room grouping.
        # ----------------------------------------------------
        specific = self._resolve_one(
            text,
            data,
            domain_hint=domain_hint,
            areas=areas,
            status=status,
        )

        if specific.get("kind") == "entity":
            score = specific.get("score", 0)

            # A very strong NAME match wins.
            if score >= 0.94:
                specific["domain"] = domain_hint
                specific["areas"] = areas
                return specific

        # ----------------------------------------------------
        # AREA GROUP
        # "tắt đèn phòng ngủ"
        # "bật đèn phòng khách"
        #
        # Use HA Area OR area inferred from NAME.
        # ----------------------------------------------------
        if areas and domain_hint:
            entities = self._entities_in_areas(
                data,
                areas,
                domain_hint,
                status=status,
            )

            if entities:
                # If only one entity exists in the requested
                # category/area, controlling it is naturally valid.
                if (
                    len(entities) == 1
                    and specific.get("kind") != "ambiguous"
                ):
                    return {
                        "kind": "entity",
                        "entities": entities,
                        "domain": domain_hint,
                        "areas": areas,
                    }

                return {
                    "kind": "area_group",
                    "entities": entities,
                    "domain": domain_hint,
                    "areas": areas,
                }

        return specific

    # --------------------------------------------------------
    # Action / service
    # --------------------------------------------------------

    def _parse_action(self, text):
        nt = norm(text)

        if re.search(r"\b(bat|mo|kich hoat|chay)\b", nt):
            if "mo" in nt and (
                "rem" in nt
                or "cua" in nt
            ):
                return "open"

            return "on"

        if re.search(r"\b(tat|dong)\b", nt):
            if "dong" in nt and (
                "rem" in nt
                or "cua" in nt
            ):
                return "close"

            return "off"

        return None

    def _is_status_query(self, text):
        nt = norm(text)

        words = (
            "kiem tra",
            "trang thai",
            "dang bat",
            "dang tat",
            "co bat",
            "co tat",
            "the nao",
            "con bat",
            "dang mo",
            "dang dong",
        )

        return any(x in nt for x in words)

    def _is_area_status(self, text):
        nt = norm(text)

        return (
            "kiem tra dien" in nt
            or "thiet bi nao" in nt
            or "con thiet bi" in nt
            or (
                "con" in nt
                and "dang bat" in nt
            )
        )

    def _needs_change(self, entity, action):
        state = str(
            entity.get("state") or ""
        ).lower()

        domain = entity.get("domain")

        if action == "on":
            if domain == "climate":
                return state == "off"

            return state not in ON_STATES

        if action == "off":
            return state not in OFF_STATES

        if action == "open":
            return state not in {
                "open",
                "opening",
            }

        if action == "close":
            return state not in {
                "closed",
                "closing",
            }

        return True

    def _service_for(self, entity, action):
        domain = entity.get("domain")

        if domain in {
            "light",
            "switch",
            "fan",
        }:
            if action in ("on", "open"):
                return domain, "turn_on"

            if action in ("off", "close"):
                return domain, "turn_off"

        if domain == "climate":
            if action in ("on", "open"):
                return "climate", "turn_on"

            if action in ("off", "close"):
                return "climate", "turn_off"

        if domain == "cover":
            if action in ("on", "open"):
                return "cover", "open_cover"

            if action in ("off", "close"):
                return "cover", "close_cover"

        if domain in {
            "script",
            "scene",
        }:
            if action in ("on", "open"):
                return domain, "turn_on"

        if domain == "automation":
            if action in ("on", "open"):
                return "automation", "trigger"

            if action == "off":
                return "automation", "turn_off"

        return None, None

    async def _execute(
        self,
        entities,
        action,
    ):
        changed = []
        already = []
        unsupported = []

        # Group by domain/service so one HA call can control many.
        batches = {}

        for e in entities:
            if not self._needs_change(
                e,
                action,
            ):
                already.append(e)
                continue

            domain, service = self._service_for(
                e,
                action,
            )

            if not domain or not service:
                unsupported.append(e)
                continue

            batches.setdefault(
                (domain, service),
                [],
            ).append(e)

        for (domain, service), batch in batches.items():
            ids = [
                e["entity_id"]
                for e in batch
            ]

            await home_assistant_registry.call_service(
                domain,
                service,
                ids,
            )

            changed.extend(batch)

        return changed, already, unsupported

    # --------------------------------------------------------
    # Replies
    # --------------------------------------------------------

    def _control_reply(
        self,
        entities,
        changed,
        already,
        action,
        resolved,
    ):
        if not changed and already:
            if len(already) == 1:
                e = already[0]

                if action in ("on", "open"):
                    state = (
                        "đang mở rồi"
                        if e.get("domain") == "cover"
                        else "đang bật rồi"
                    )
                else:
                    state = (
                        "đang đóng rồi"
                        if e.get("domain") == "cover"
                        else "đang tắt rồi"
                    )

                return f"{e['name']} {state}."

            if resolved.get("kind") == "area_group":
                areas = resolved.get("areas") or []

                if len(areas) == 1:
                    area = areas[0]["name"]

                    return (
                        f"Các thiết bị yêu cầu ở "
                        f"{area} đã ở đúng trạng thái rồi."
                    )

            return (
                f"{len(already)} thiết bị đã ở "
                f"đúng trạng thái rồi."
            )

        verb = {
            "on": "Đã bật",
            "off": "Đã tắt",
            "open": "Đã mở",
            "close": "Đã đóng",
        }.get(action, "Đã thực hiện")

        if len(entities) == 1:
            return f"{verb} {entities[0]['name']}."

        if resolved.get("kind") == "area_group":
            areas = resolved.get("areas") or []
            domain = resolved.get("domain")

            if len(areas) == 1:
                area = areas[0]["name"]

                noun = {
                    "light": "đèn",
                    "fan": "quạt",
                    "switch": "thiết bị",
                    "cover": "rèm",
                    "climate": "điều hòa",
                }.get(
                    domain,
                    "thiết bị",
                )

                return f"{verb} {noun} {area}."

        # Natural reply for a small multi-target command.
        if changed and len(changed) <= 3:
            names = [
                e.get("name") or e.get("entity_id")
                for e in changed
            ]

            if len(names) == 1:
                joined = names[0]
            elif len(names) == 2:
                joined = f"{names[0]} và {names[1]}"
            else:
                joined = (
                    ", ".join(names[:-1])
                    + f" và {names[-1]}"
                )

            return f"{verb} {joined}."

        return f"{verb} {len(changed)} thiết bị."

    def _status_reply(self, entities):
        if not entities:
            return "Tôi chưa tìm thấy thiết bị phù hợp."

        if len(entities) == 1:
            e = entities[0]
            return (
                f"{e['name']} "
                f"{human_state(e)}."
            )

        active = [
            e for e in entities
            if str(e.get("state") or "").lower()
            in ON_STATES
        ]

        if not active:
            return (
                "Các thiết bị được hỏi "
                "hiện đều đang tắt."
            )

        names = [
            e.get("name")
            for e in active[:5]
        ]

        joined = ", ".join(names)

        return (
            f"Hiện có {len(active)} thiết bị "
            f"đang hoạt động: {joined}."
        )

    def _area_status_reply(
        self,
        entities,
        areas,
    ):
        active = [
            e for e in entities
            if str(e.get("state") or "").lower()
            in ON_STATES
        ]

        area_name = (
            areas[0].get("name")
            if len(areas) == 1
            else None
        )

        if not active:
            if area_name:
                return (
                    f"{area_name} hiện không có "
                    "thiết bị nào đang bật."
                )

            return "Hiện không có thiết bị nào đang bật."

        names = [
            e.get("name")
            for e in active[:6]
        ]

        if len(names) == 1:
            listing = names[0]
        elif len(names) == 2:
            listing = f"{names[0]} và {names[1]}"
        else:
            listing = (
                ", ".join(names[:-1])
                + f" và {names[-1]}"
            )

        if area_name:
            return (
                f"{area_name} hiện có "
                f"{len(active)} thiết bị đang bật: "
                f"{listing}."
            )

        return (
            f"Hiện có {len(active)} thiết bị "
            f"đang bật: {listing}."
        )

    # --------------------------------------------------------
    # Main
    # --------------------------------------------------------

    async def handle(self, intent, ctx):
        status = await home_assistant_connection.status(
            max_age=3
        )

        if not status.get("configured"):
            return SkillResult(
                True,
                "Home Assistant chưa được cấu hình.",
            )

        if not status.get("connected"):
            return SkillResult(
                True,
                "Home Assistant hiện không kết nối được.",
            )

        slots = intent.slots or {}

        text = str(
            slots.get("text")
            or getattr(intent, "text", "")
            or ""
        ).strip()

        if not text:
            return SkillResult(
                True,
                "Tôi chưa nghe rõ thiết bị cần điều khiển.",
            )

        nt = norm(text)

        if any(
            norm(x) == nt
            for x in CANCEL_WORDS
        ):
            self._clear_pending(ctx)
            return SkillResult(
                True,
                "Đã hủy.",
            )

        try:
            data = await self._data(
                refresh=True
            )

            # ----------------------------------------------
            # AREA STATUS
            # ----------------------------------------------
            if (
                intent.name == "home.area_status"
                or self._is_area_status(text)
            ):
                areas = self._find_areas_in_text(
                    text,
                    data,
                )

                if not areas:
                    self._set_pending(
                        ctx,
                        {
                            "mode": "area_status",
                            "text": text,
                            "missing": "area",
                        },
                    )

                    return SkillResult(
                        True,
                        "Bạn muốn kiểm tra phòng nào?",
                    )

                entities = self._entities_in_areas(
                    data,
                    areas,
                    status=True,
                )

                return SkillResult(
                    True,
                    self._area_status_reply(
                        entities,
                        areas,
                    ),
                )

            # ----------------------------------------------
            # DEVICE STATE
            # ----------------------------------------------
            if (
                intent.name == "home.state"
                or self._is_status_query(text)
            ):
                resolved = self._resolve_targets(
                    text,
                    data,
                    status=True,
                )

                entities = resolved.get(
                    "entities",
                    [],
                )

                if not entities:
                    self._set_pending(
                        ctx,
                        {
                            "mode": "state",
                            "text": text,
                            "missing": "target",
                        },
                    )

                    return SkillResult(
                        True,
                        "Bạn muốn kiểm tra thiết bị nào?",
                    )

                if resolved.get("kind") == "ambiguous":
                    names = [
                        e.get("name")
                        for e in entities[:2]
                    ]

                    self._set_pending(
                        ctx,
                        {
                            "mode": "state",
                            "text": text,
                            "missing": "target",
                        },
                    )

                    return SkillResult(
                        True,
                        "Bạn muốn kiểm tra "
                        + " hay ".join(names)
                        + "?",
                    )

                return SkillResult(
                    True,
                    self._status_reply(entities),
                )

            # ----------------------------------------------
            # CONTROL
            # ----------------------------------------------
            action = (
                slots.get("action")
                or self._parse_action(text)
            )

            # Convert old manifest on/off if supplied.
            if action == "on":
                action = "on"
            elif action == "off":
                action = "off"

            if not action:
                return SkillResult(
                    True,
                    "Tôi chưa xác định được thao tác cần thực hiện.",
                )

            resolved = self._resolve_targets(
                text,
                data,
                status=False,
            )

            entities = resolved.get(
                "entities",
                [],
            )

            if (
                resolved.get("kind") == "ambiguous"
                and entities
            ):
                names = [
                    e.get("name")
                    for e in entities[:2]
                ]

                self._set_pending(
                    ctx,
                    {
                        "mode": "control",
                        "action": action,
                        "text": text,
                        "missing": "target",
                    },
                )

                return SkillResult(
                    True,
                    "Bạn muốn điều khiển "
                    + " hay ".join(names)
                    + "?",
                )

            if not entities:
                domain = self._domain_hint(text)

                self._set_pending(
                    ctx,
                    {
                        "mode": "control",
                        "action": action,
                        "domain": domain,
                        "text": text,
                        "missing": "target",
                    },
                )

                noun = {
                    "light": "đèn",
                    "fan": "quạt",
                    "climate": "điều hòa",
                    "cover": "rèm",
                    "switch": "thiết bị",
                }.get(
                    domain,
                    "thiết bị",
                )

                return SkillResult(
                    True,
                    f"Bạn muốn điều khiển {noun} nào?",
                )

            changed, already, unsupported = (
                await self._execute(
                    entities,
                    action,
                )
            )

            self._clear_pending(ctx)

            if not changed and not already:
                return SkillResult(
                    True,
                    "Thiết bị này chưa hỗ trợ thao tác đó.",
                )

            return SkillResult(
                True,
                self._control_reply(
                    entities,
                    changed,
                    already,
                    action,
                    resolved,
                ),
            )

        except Exception as e:
            print(
                "[HA SKILL ERROR]",
                type(e).__name__,
                str(e),
                flush=True,
            )

            return SkillResult(
                True,
                "Home Assistant hiện không thực hiện được lệnh.",
            )


# ============================================================
# MANIFEST
# Understanding only identifies HA intent.
# Entity understanding is owned by this skill.
# ============================================================

MANIFEST = {
    "name": "home_assistant",

    "intents": {
        "home.area_status": {
            "patterns": [
                {
                    "regex": (
                        r"\bkiểm tra điện\b.*"
                    ),
                },
                {
                    "regex": (
                        r"\b(phòng|phong)\b.*"
                        r"\b(còn|con)\b.*"
                        r"\b(thiết bị|thiet bi|đang bật|dang bat)\b"
                    ),
                },
            ],

            "slots": {
                "text": "copy_text",
                "room": "copy_room",
            },
        },

        "home.state": {
            "patterns": [
                {
                    "regex": (
                        r"\b(kiểm tra|trạng thái|xem)\b.*"
                        r"\b(đèn|ổ cắm|công tắc|quạt|"
                        r"điều hòa|máy lạnh|rèm|thiết bị)\b"
                    ),
                },
                {
                    "regex": (
                        r"\b(đèn|ổ cắm|công tắc|quạt|"
                        r"điều hòa|máy lạnh|rèm)\b.*"
                        r"\b(đang bật|đang tắt|có bật|có tắt|"
                        r"đang mở|đang đóng|thế nào)\b"
                    ),
                },
            ],

            "slots": {
                "text": "copy_text",
                "room": "copy_room",
            },
        },

        "home.control": {
            "patterns": [
                {
                    "regex": (
                        r"\b(bật|mở|chạy|kích hoạt)\b.*"
                        r"\b(đèn|ổ cắm|công tắc|quạt|"
                        r"điều hòa|máy lạnh|rèm|"
                        r"chế độ|kịch bản|tự động)\b"
                    ),
                },
                {
                    "regex": (
                        r"\b(tắt|đóng)\b.*"
                        r"\b(đèn|ổ cắm|công tắc|quạt|"
                        r"điều hòa|máy lạnh|rèm|thiết bị)\b"
                    ),
                },
                {
                    "regex": (
                        r"\b(tắt|bật)\b.*"
                        r"\b(tất cả|toàn bộ|hết)\b.*"
                    ),
                },
            ],

            "slots": {
                "text": "copy_text",
                "room": "copy_room",
            },
        },
    },
}
