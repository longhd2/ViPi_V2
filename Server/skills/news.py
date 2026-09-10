import asyncio
import re
import feedparser
from .base import Skill, SkillResult


class NewsSkill(Skill):
    name = "news"
    priority = 70

    DEFAULT_FEED = "https://vnexpress.net/rss/tin-moi-nhat.rss"

    def _feed_url(self, text):
        if "công nghệ" in text:
            return "https://vnexpress.net/rss/so-hoa.rss"
        if "thế giới" in text:
            return "https://vnexpress.net/rss/the-gioi.rss"
        if "kinh tế" in text:
            return "https://vnexpress.net/rss/kinh-doanh.rss"
        return self.DEFAULT_FEED

    def _clean_title(self, title):
        title = re.sub(r"<[^>]+>", "", title)
        return re.sub(r"\s+", " ", title).strip()

    async def handle(self, intent, ctx):
        text = intent.slots.get("text", "").lower()
        url = self._feed_url(text)

        try:
            feed = await asyncio.to_thread(
                feedparser.parse,
                url,
            )
        except Exception:
            return SkillResult(
                True,
                "Hiện tôi chưa lấy được tin tức.",
            )

        if not feed.entries:
            return SkillResult(
                True,
                "Hiện tôi chưa lấy được tin tức.",
            )

        titles = []

        for entry in feed.entries[:3]:
            title = self._clean_title(entry.get("title", ""))
            if title:
                titles.append(title)

        if not titles:
            return SkillResult(
                True,
                "Hiện tôi chưa lấy được tin tức.",
            )

        if len(titles) == 1:
            reply = f"Tin mới nhất: {titles[0]}."
        else:
            reply = ". ".join(
                f"Tin {i}: {title}"
                for i, title in enumerate(titles, 1)
            ) + "."

        return SkillResult(True, reply)


MANIFEST = {
    "name": "news",
    "intents": {
        "news.latest": {
            "phrases": [
                "tin tức",
                "tin mới",
                "tin hôm nay",
                "có tin gì",
                "đọc tin",
                "tin mới nhất",
                "tin công nghệ",
                "tin thế giới",
                "tin kinh tế",
            ],
            "copy_text": True,
        }
    },
}
