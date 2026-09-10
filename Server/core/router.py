import logging

log = logging.getLogger("ViPiV2.Router")


class Router:
    """
    One utterance
       -> Multi Intent Plan
       -> direct O(1) skill dispatch
       -> normal intents execute in spoken order
       -> media.play deferred until after TTS
       -> individual responses preserved

    NO LLM.
    """

    DEFERRED_INTENTS = {
        "media.play",
    }

    def __init__(
        self,
        understanding,
        registry
    ):
        self.understanding = understanding
        self.registry = registry

    async def handle_text(
        self,
        text,
        ctx
    ):
        plan = self.understanding.resolve(
            text,
            ctx
        )

        if not plan.intents:
            intent = self.understanding.match(
                text,
                ctx
            )

            step = {
                "intent": intent,
                "ok": False,
                "reply": "Tôi chưa hiểu yêu cầu đó.",
                "data": {},
                "deferred": False,
            }

            return {
                "plan": plan,
                "intents": [],
                "intent": intent,
                "reply": step["reply"],
                "replies": [step["reply"]],
                "data": {},
                "steps": [step],
                "deferred": [],
            }

        steps = []
        deferred = []
        replies = []
        all_data = []

        for intent in plan.intents:

            # media.play must not start before TTS finishes.
            if intent.name in self.DEFERRED_INTENTS:
                deferred.append(intent)

                steps.append(
                    {
                        "intent": intent,
                        "ok": True,
                        "reply": "",
                        "data": {},
                        "deferred": True,
                    }
                )

                log.info(
                    "DEFER %s until after TTS",
                    intent.name
                )

                continue

            result = await self.registry.dispatch(
                intent,
                ctx
            )

            if result is None:
                steps.append(
                    {
                        "intent": intent,
                        "ok": False,
                        "reply": "",
                        "data": {},
                        "deferred": False,
                    }
                )
                continue

            ok = bool(
                getattr(
                    result,
                    "handled",
                    getattr(result, "ok", True)
                )
            )

            reply = (
                getattr(
                    result,
                    "reply",
                    ""
                )
                or ""
            ).strip()

            data = (
                getattr(
                    result,
                    "data",
                    {}
                )
                or {}
            )

            if reply:
                replies.append(reply)

            if data:
                all_data.append(data)

            steps.append(
                {
                    "intent": intent,
                    "ok": ok,
                    "reply": reply,
                    "data": data,
                    "deferred": False,
                }
            )

        # Compatibility only.
        # Runtime V2 uses "replies", not this aggregated string.
        final_reply = " ".join(replies).strip()

        return {
            "plan": plan,
            "intents": plan.intents,
            "intent": plan.intents[0],
            "reply": final_reply,
            "replies": replies,
            "data": (
                all_data[0]
                if len(all_data) == 1
                else {
                    "actions": all_data
                }
                if all_data
                else {}
            ),
            "steps": steps,
            "deferred": deferred,
        }

    async def dispatch_deferred(
        self,
        routed,
        ctx
    ):
        """
        Execute intents deliberately delayed until all TTS replies
        for the utterance have completed.
        """

        results = []

        for intent in routed.get("deferred", []):
            log.info(
                "DISPATCH DEFERRED %s",
                intent.name
            )

            result = await self.registry.dispatch(
                intent,
                ctx
            )

            results.append(
                {
                    "intent": intent,
                    "result": result,
                }
            )

        return results
