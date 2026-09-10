from dataclasses import dataclass, field
from difflib import SequenceMatcher
import re

from .normalize import normalize


@dataclass
class Intent:
    name: str
    slots: dict = field(default_factory=dict)
    confidence: float = 0.0
    skill: str = ""
    order: int = 0
    source: str = ""
    span: tuple = (0, 0)


@dataclass
class IntentPlan:
    text: str
    normalized: str
    intents: list[Intent] = field(
        default_factory=list
    )

    @property
    def confidence(self):
        if not self.intents:
            return 0.0

        return min(
            x.confidence
            for x in self.intents
        )


class UnderstandingEngine:
    """
    Deterministic Multi-Intent Engine.
    NO LLM.

    Flow:
      normalize
      -> indexed phrase matching
      -> indexed regex patterns
      -> lightweight fuzzy/semantic
      -> slot extraction
      -> ordered execution plan

    Engine khong vao tung skill de hoi match().
    """

    def __init__(self, intent_specs=None):
        self.intent_specs = {}
        self.phrase_index = []
        self.pattern_index = []

        if intent_specs:
            self.build_index(
                intent_specs
            )

    def build_index(self, intent_specs):
        self.intent_specs = dict(
            intent_specs or {}
        )

        self.phrase_index = []
        self.pattern_index = []

        for intent_name, spec in self.intent_specs.items():
            skill = spec.get("skill", "")

            for phrase in spec.get(
                "phrases",
                []
            ):
                p = normalize(phrase)

                if not p:
                    continue

                self.phrase_index.append(
                    {
                        "phrase": p,
                        "intent": intent_name,
                        "skill": skill,
                        "spec": spec,
                    }
                )

            for pattern in spec.get(
                "patterns",
                []
            ):
                regex = pattern.get(
                    "regex"
                )

                if not regex:
                    continue

                self.pattern_index.append(
                    {
                        "regex": re.compile(
                            regex,
                            re.IGNORECASE
                        ),
                        "intent": intent_name,
                        "skill": skill,
                        "spec": spec,
                        "pattern": pattern,
                    }
                )

        self.phrase_index.sort(
            key=lambda x: len(x["phrase"]),
            reverse=True
        )

    def _phrase_hits(self, text):
        hits = []

        for item in self.phrase_index:
            phrase = item["phrase"]

            start = 0

            while True:
                pos = text.find(
                    phrase,
                    start
                )

                if pos < 0:
                    break

                end = pos + len(phrase)

                hits.append(
                    {
                        **item,
                        "start": pos,
                        "end": end,
                        "confidence": 0.97,
                        "source": "phrase",
                        "matched": phrase,
                    }
                )

                start = end

        return hits

    def _pattern_hits(self, text):
        hits = []

        for item in self.pattern_index:
            for m in item["regex"].finditer(
                text
            ):
                hits.append(
                    {
                        **item,
                        "start": m.start(),
                        "end": m.end(),
                        "confidence": 0.90,
                        "source": "pattern",
                        "matched": m.group(0),
                    }
                )

        return hits

    def _apply_exclusive_hits(self, hits):
        """
        Wrapper intent co exclusive=True se chiem cac hit
        nam ben trong span cua no.

        Vi du:
          "10 phut nua tat nhac"

        schedule.action bao toan cau, nen media.stop
        khong duoc thuc thi ngay.
        """
        exclusive = [
            h for h in hits
            if h.get("spec", {}).get("exclusive")
        ]

        if not exclusive:
            return hits

        result = []

        for hit in hits:
            if hit in exclusive:
                result.append(hit)
                continue

            covered = False

            for wrapper in exclusive:
                if (
                    wrapper["start"] <= hit["start"]
                    and wrapper["end"] >= hit["end"]
                ):
                    covered = True
                    break

            if not covered:
                result.append(hit)

        return result

    def _remove_duplicate_hits(self, hits):
        """
        Mot intent chi lay match tot nhat gan vi tri do.
        Phrase dai uu tien hon phrase ngan bi long ben trong.
        """

        hits = sorted(
            hits,
            key=lambda h: (
                h["start"],
                -(
                    h["end"]
                    - h["start"]
                ),
                -h["confidence"],
            )
        )

        result = []

        for hit in hits:
            duplicate = False

            for old in result:
                if old["intent"] != hit["intent"]:
                    continue

                overlap = not (
                    hit["end"] <= old["start"]
                    or
                    hit["start"] >= old["end"]
                )

                if overlap:
                    duplicate = True
                    break

            if not duplicate:
                result.append(hit)

        return sorted(
            result,
            key=lambda h: h["start"]
        )

    def _best_fuzzy(self, clause):
        best = None
        best_score = 0.0

        c = clause.strip()

        if len(c) < 3:
            return None

        c_tokens = set(c.split())

        for item in self.phrase_index:
            phrase = item["phrase"]

            seq = SequenceMatcher(
                None,
                c,
                phrase
            ).ratio()

            p_tokens = set(
                phrase.split()
            )

            union = (
                c_tokens
                | p_tokens
            )

            token_score = (
                len(
                    c_tokens
                    & p_tokens
                )
                / len(union)
                if union
                else 0.0
            )

            score = (
                seq * 0.65
                + token_score * 0.35
            )

            if score > best_score:
                best_score = score
                best = item

        if (
            best is None
            or best_score < 0.72
        ):
            return None

        pos = clause.find(
            best["phrase"]
        )

        if pos < 0:
            pos = 0

        return {
            **best,
            "start": pos,
            "end": pos + len(clause),
            "confidence": round(
                best_score,
                3
            ),
            "source": "fuzzy",
            "matched": clause,
        }

    def _fuzzy_hits(self, text):
        parts = []

        pattern = re.compile(
            r"\s*(?:,|;|\brồi\b|\bsau đó\b|\bvà\b)\s*",
            re.IGNORECASE
        )

        cursor = 0

        for m in pattern.finditer(text):
            clause = text[
                cursor:m.start()
            ].strip()

            if clause:
                parts.append(
                    (
                        cursor,
                        clause
                    )
                )

            cursor = m.end()

        clause = text[cursor:].strip()

        if clause:
            parts.append(
                (
                    cursor,
                    clause
                )
            )

        hits = []

        for offset, clause in parts:
            hit = self._best_fuzzy(
                clause
            )

            if not hit:
                continue

            hit["start"] += offset
            hit["end"] = (
                offset
                + len(clause)
            )

            hits.append(hit)

        return hits

    def _clean_query(self, value):
        value = value.strip(
            " ,.;:-"
        )

        value = re.sub(
            r"^(cho tôi|cho mình)\s+",
            "",
            value
        )

        value = re.sub(
            r"\s+(và|rồi|sau đó)\s*$",
            "",
            value
        )

        return value.strip()

    def _duration_seconds(self, text):
        total = 0

        units = {
            "giây": 1,
            "phút": 60,
            "tiếng": 3600,
            "giờ": 3600,
        }

        for value, unit in re.findall(
            r"(\d+)\s*(giây|phút|tiếng|giờ)",
            text
        ):
            total += (
                int(value)
                * units[unit]
            )

        return total

    def _slots(
        self,
        hit,
        text,
        segment_end,
        ctx
    ):
        spec = hit.get(
            "spec",
            {}
        )

        slots = {}

        pattern = hit.get(
            "pattern"
        )

        if pattern:
            slots.update(
                pattern.get(
                    "slots",
                    {}
                )
            )

        if spec.get("copy_text"):
            slots["text"] = text

        if spec.get("copy_room") and ctx:
            slots["room"] = getattr(
                ctx,
                "room",
                "default"
            )

        slot_defs = spec.get(
            "slots",
            {}
        )

        for name, extractor in slot_defs.items():
            if extractor == "copy_text":
                slots[name] = text

            elif extractor == "copy_room":
                slots[name] = getattr(
                    ctx,
                    "room",
                    "default"
                ) if ctx else "default"

            elif extractor == "after_phrase":
                value = text[
                    hit["end"]:segment_end
                ]

                slots[name] = (
                    self._clean_query(
                        value
                    )
                )

            elif extractor == "duration_seconds":
                seconds = self._duration_seconds(
                    text[
                        hit["start"]:segment_end
                    ]
                )

                if seconds:
                    slots[name] = seconds

        return slots

    def resolve(self, text, ctx=None):
        t = normalize(text)

        if not t:
            return IntentPlan(
                text=text,
                normalized=t,
                intents=[]
            )

        hits = (
            self._phrase_hits(t)
            + self._pattern_hits(t)
        )

        hits = self._remove_duplicate_hits(
            hits
        )

        hits = self._apply_exclusive_hits(
            hits
        )

        if not hits:
            hits = self._fuzzy_hits(
                t
            )

            hits = self._remove_duplicate_hits(
                hits
            )

        intents = []

        for index, hit in enumerate(hits):
            next_start = (
                hits[index + 1]["start"]
                if index + 1 < len(hits)
                else len(t)
            )

            segment_end = max(
                hit["end"],
                next_start
            )

            slots = self._slots(
                hit,
                t,
                segment_end,
                ctx
            )

            intents.append(
                Intent(
                    name=hit["intent"],
                    slots=slots,
                    confidence=hit[
                        "confidence"
                    ],
                    skill=hit.get(
                        "skill",
                        ""
                    ),
                    order=len(intents) + 1,
                    source=hit[
                        "source"
                    ],
                    span=(
                        hit["start"],
                        hit["end"]
                    ),
                )
            )

        return IntentPlan(
            text=text,
            normalized=t,
            intents=intents
        )

    def match_all(self, text, ctx=None):
        return self.resolve(
            text,
            ctx
        ).intents

    def match(self, text, ctx=None):
        """
        Compatibility API cho test/cu code.
        """

        intents = self.match_all(
            text,
            ctx
        )

        if intents:
            return intents[0]

        return Intent(
            "unknown",
            {"text": normalize(text)},
            0.0
        )
