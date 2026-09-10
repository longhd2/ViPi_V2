from .base import Skill, SkillResult


class MediaSkill(Skill):
    name = "media"
    priority = 30

    async def match(self, intent, ctx):
        return False

    async def handle(self, intent, ctx):
        from media.manager import media_manager

        if intent.name == "media.play":
            query = (
                intent.slots.get("query", "")
                or ""
            ).strip()

            if not query:
                return SkillResult(
                    False,
                    "Bạn muốn nghe bài gì?"
                )

            await media_manager.play(
                query,
                ctx
            )

            return SkillResult(
                True,
                ""
            )

        if intent.name == "media.stop":
            await media_manager.stop(ctx)

            return SkillResult(
                True,
                ""
            )

        if intent.name == "media.pause":
            pause = getattr(
                media_manager,
                "pause",
                None
            )

            if pause:
                await pause(ctx)

            return SkillResult(
                True,
                ""
            )

        if intent.name == "media.resume":
            resume = getattr(
                media_manager,
                "resume",
                None
            )

            if resume:
                await resume(ctx)

            return SkillResult(
                True,
                ""
            )

        return SkillResult(False)


MANIFEST = {
    "name": "media",

    "intents": {
        "media.play": {
            "phrases": [
                "mở nhạc",
                "phát nhạc",
                "bật nhạc",
                "phát bài hát",
                "mở bài hát",
                "bật bài hát",
                "nghe bài",
                "nghe bài hát",
            ],

            "slots": {
                "query": "after_phrase"
            },
        },

        "media.stop": {
            "phrases": [
                "dừng nhạc",
                "tắt nhạc",
                "ngừng nhạc",
                "dừng bài hát",
            ]
        },

        "media.pause": {
            "phrases": [
                "tạm dừng nhạc",
                "tạm dừng bài hát",
                "pause nhạc",
            ]
        },

        "media.resume": {
            "phrases": [
                "phát tiếp",
                "tiếp tục nhạc",
                "tiếp tục bài hát",
            ]
        },
    },
}
