import asyncio
from datetime import datetime, timezone
from typing import AsyncIterator
from urllib.parse import quote

from src.common.models import UnifiedItem
from src.wheel.browser.browser_manager import BrowserManager
from src.wheel.logger import get_logger

from ..base_adapter import BaseAdapter
from ..adapter_models import AdapterConfig

# RedNote DOM selectors (class-level constants, not in config)
_NOTE_ITEM = "section.note-item, .note-item, article"
_NOTE_TITLE = ".title span, .note-title span, .desc, h3 a"
_NOTE_AUTHOR = ".author .name, .name, .nickname, .username"
_NOTE_CONTENT = ".desc, .note-text, .content"
_NOTE_LIKES = ".like-wrapper .count, .count, .like-count"
_NOTE_COVER = ".cover img, .img-container img, .feeds-page img"
_NOTE_LINK = "a[href*='/explore/'], a[href*='/discovery/']"

# RedNote URL constants (platform-specific, not in AdapterConfig)
_SEARCH_URL = "https://www.xiaohongshu.com/search_result?keyword={keyword}"
_USER_URL_TEMPLATE = "https://www.xiaohongshu.com/user/profile/{user_id}"
_TRENDING_URL = "https://www.xiaohongshu.com/explore"
_BASE_URL = "https://www.xiaohongshu.com"


class RedNoteAdapter(BaseAdapter):
    platform_name = "rednote"

    def __init__(self, browser_manager: BrowserManager, config: AdapterConfig,
                 *, search_url: str | None = None,
                 user_url_template: str | None = None,
                 trending_url: str | None = None,
                 base_url: str | None = None):
        self._browser = browser_manager
        self._config = config
        self._log = get_logger("rednote")
        self._search_url = search_url if search_url is not None else _SEARCH_URL
        self._user_url_template = user_url_template if user_url_template is not None else _USER_URL_TEMPLATE
        self._trending_url = trending_url if trending_url is not None else _TRENDING_URL
        self._base_url = base_url if base_url is not None else _BASE_URL

    async def search(self, keywords: list[str], limit: int) -> AsyncIterator[UnifiedItem]:
        ctx = await self._browser.get_context(self.platform_name)
        page = await ctx.new_page()
        try:
            await self._inject_stealth(page)
            keyword = " ".join(keywords)
            url = self._search_url.replace("{keyword}", quote(keyword))
            await page.goto(url, wait_until="domcontentloaded")
            async for item in self._scroll_and_extract(page, limit):
                yield item
        finally:
            await page.close()

    async def get_user_posts(self, user_id: str, limit: int) -> AsyncIterator[UnifiedItem]:
        ctx = await self._browser.get_context(self.platform_name)
        page = await ctx.new_page()
        try:
            await self._inject_stealth(page)
            url = self._user_url_template.replace("{user_id}", quote(user_id))
            await page.goto(url, wait_until="domcontentloaded")
            async for item in self._scroll_and_extract(page, limit):
                yield item
        finally:
            await page.close()

    async def get_trending(self, limit: int) -> AsyncIterator[UnifiedItem]:
        ctx = await self._browser.get_context(self.platform_name)
        page = await ctx.new_page()
        try:
            await self._inject_stealth(page)
            await page.goto(self._trending_url, wait_until="domcontentloaded")
            async for item in self._scroll_and_extract(page, limit):
                yield item
        finally:
            await page.close()

    def extract(self, data: dict) -> UnifiedItem:
        return UnifiedItem(
            platform=self.platform_name,
            item_id=str(data.get("item_id", data.get("id", ""))),
            item_type=data.get("item_type", "post"),
            author_id=str(data.get("author_id", data.get("user_id", ""))),
            author_name=str(data.get("author_name", data.get("username", ""))),
            content_text=str(data.get("content", data.get("title", ""))),
            content_media=data.get("images", data.get("media", [])),
            url=str(data.get("url", "")),
            published_at=str(data.get("published_at", data.get("time", ""))),
            fetched_at=datetime.now(timezone.utc).isoformat(),
            metrics={
                "likes": data.get("likes", 0),
                "comments": data.get("comments", 0),
                "shares": data.get("shares", 0),
                "collects": data.get("collects", 0),
            },
        )

    async def health_check(self) -> bool:
        try:
            ctx = await self._browser.get_context(self.platform_name)
            page = await ctx.new_page()
            try:
                await page.goto(self._base_url, wait_until="domcontentloaded")
                return True
            finally:
                await page.close()
        except Exception:
            self._log.warning("health check failed for rednote")
            return False

    # ── private ──

    async def _scroll_and_extract(self, page, limit: int) -> AsyncIterator[UnifiedItem]:
        pw = page.raw()
        extracted: set[str] = set()
        prev_count = 0
        stall_streak = 0

        while stall_streak < 3 and len(extracted) < limit:
            items = await pw.query_selector_all(_NOTE_ITEM)
            for el in items:
                if len(extracted) >= limit:
                    break
                data = await self._scrape_element(el)
                if data is None:
                    continue
                uid = data.get("item_id", "")
                if uid and uid not in extracted:
                    extracted.add(uid)
                    yield self.extract(data)

            if len(extracted) >= limit:
                break

            await pw.evaluate("window.scrollBy(0, window.innerHeight)")
            await asyncio.sleep(self._config.scroll_delay)

            new_items = await pw.query_selector_all(_NOTE_ITEM)
            if len(new_items) <= prev_count:
                stall_streak += 1
            else:
                stall_streak = 0
            prev_count = len(new_items)

    async def _scrape_element(self, element) -> dict | None:
        try:
            title = await element.eval_on_selector(_NOTE_TITLE, "el => el?.textContent?.trim() || ''")
            author = await element.eval_on_selector(_NOTE_AUTHOR, "el => el?.textContent?.trim() || ''")
            content = await element.eval_on_selector(_NOTE_CONTENT, "el => el?.textContent?.trim() || ''")
            likes_text = await element.eval_on_selector(
                _NOTE_LIKES, "el => el?.textContent?.trim() || '0'"
            )

            link = await element.eval_on_selector(
                _NOTE_LINK, "el => el?.getAttribute('href') || ''"
            )
            cover_srcs = await element.eval_on_selector_all(
                _NOTE_COVER, "els => els.map(el => el.getAttribute('src') || '').filter(Boolean)"
            )

            return {
                "item_id": await element.get_attribute("data-id") or title or "",
                "author_name": author,
                "title": title,
                "content": content,
                "url": link if link.startswith("http") else f"{self._base_url}{link}",
                "likes": self._parse_int(likes_text),
                "images": cover_srcs,
            }
        except Exception:
            return None

    async def _inject_stealth(self, page) -> None:
        try:
            from playwright_stealth import Stealth

            pw = page.raw()
            await Stealth().apply_stealth_async(pw)
        except ImportError:
            self._log.debug("playwright_stealth not available, skipping")

    @staticmethod
    def _parse_int(s: str) -> int:
        s = s.strip().replace(",", "")
        if s.endswith("万"):
            try:
                return int(float(s[:-1]) * 10000)
            except ValueError:
                return 0
        if s.endswith("k"):
            try:
                return int(float(s[:-1]) * 1000)
            except ValueError:
                return 0
        try:
            return int(float(s))
        except ValueError:
            return 0
