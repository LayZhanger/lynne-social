import pytest

from src.core.adapters.adapter_models import AdapterConfig
from src.core.adapters.imp.rednote_adapter import RedNoteAdapter
from src.wheel.browser.browser_factory import BrowserManagerFactory
from src.wheel.browser.browser_models import BrowserConfig
from src.common.models import UnifiedItem


def _build_test_html(n: int, start_hidden: int = 5, batch: int = 5) -> str:
    items_html = []
    for i in range(1, n + 1):
        hidden = ' style="display:none"' if i > start_hidden else ""
        items_html.append(f"""\
<section class="note-item" data-id="{i}"{hidden}>
  <a class="cover" href="/explore/item_{i}">
    <img src="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'><rect fill='%23eee' width='100' height='100'/></svg>" />
  </a>
  <div class="title"><span>帖子标题 #{i}</span></div>
  <div class="author"><span class="name">用户{i}</span></div>
  <div class="desc">这是第{i}条笔记的正文内容。AI技术正在快速发展。</div>
  <div class="like-wrapper"><span class="count">{i * 10}</span></div>
</section>""")
    return f"""<!DOCTYPE html>
<html lang="zh"><head><meta charset="UTF-8"><title>RedNote Test</title>
<style>
  body {{ font-family: sans-serif; background: #f5f5f5; }}
  section.note-item {{ border: 1px solid #ddd; margin: 20px 10px; padding: 15px; min-height: 180px; background: #fff; }}
  .title span {{ font-size: 16px; font-weight: bold; display: block; }}
  .author .name {{ color: #999; font-size: 13px; }}
  .desc {{ margin: 8px 0; font-size: 14px; color: #333; }}
  .like-wrapper .count {{ color: #ff4d4f; font-size: 13px; }}
  .cover img {{ width: 120px; height: 120px; object-fit: cover; }}
</style></head><body>
<div id="feed">{"".join(items_html)}</div>
<script>
(function() {{
  let all = document.querySelectorAll('.note-item');
  let visible = {start_hidden};
  window.addEventListener('scroll', function() {{
    if (window.innerHeight + window.scrollY >= document.body.scrollHeight - 80) {{
      let next = Math.min(visible + {batch}, all.length);
      for (let i = visible; i < next; i++) all[i].style.display = '';
      visible = next;
    }}
  }});
}})();
</script></body></html>"""


def _empty_page_html() -> str:
    return """<!DOCTYPE html>
<html lang="zh"><head><meta charset="UTF-8"></head><body>
<h1>No results found</h1><div id="feed"></div>
</body></html>"""


def _health_check_html() -> str:
    return "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>OK</h1></body></html>"


# ═════════════════════════════════════════════════════════════════
# TA: RedNoteAdapter with real Chromium headless
# ═════════════════════════════════════════════════════════════════

class TestRedNoteAdapterTA:
    @pytest.fixture(autouse=True)
    def _setup(self, tmp_path):
        self._tmp_path = tmp_path

    # ── helpers ──

    async def _make_adapter(self, **kw) -> tuple[RedNoteAdapter, object]:
        browser_cfg = BrowserConfig(
            headless=True,
            sessions_dir=str(self._tmp_path / "sessions"),
        )
        browser = BrowserManagerFactory().create(browser_cfg)
        await browser.start()

        url_fields = {"search_url", "base_url", "user_url_template", "trending_url"}
        url_kwargs = {k: kw.pop(k) for k in url_fields if k in kw}

        cfg_kw = {
            "platform": "rednote",
            "scroll_delay": 0.05,
            "max_scrolls": 15,
        }
        cfg_kw.update(kw)
        return RedNoteAdapter(browser, AdapterConfig(**cfg_kw), **url_kwargs), browser

    def _write_page(self, name: str, content: str) -> str:
        p = self._tmp_path / name
        p.write_text(content, encoding="utf-8")
        return f"file://{p}"

    # ── extract ──

    def test_extract_maps_all_fields(self):
        data = {
            "item_id": "N001",
            "item_type": "video",
            "author_id": "U123",
            "author_name": "测试用户",
            "title": "AI 进展",
            "content": "正文内容",
            "url": "https://www.xiaohongshu.com/explore/N001",
            "images": ["img1.jpg", "img2.jpg"],
            "time": "2026-04-25T10:00:00Z",
            "likes": 1234,
            "comments": 56,
            "shares": 78,
            "collects": 90,
        }
        adapter = RedNoteAdapter.__new__(RedNoteAdapter)
        adapter.platform_name = "rednote"

        item = adapter.extract(data)
        assert isinstance(item, UnifiedItem)
        assert item.platform == "rednote"
        assert item.item_id == "N001"
        assert item.item_type == "video"
        assert item.author_id == "U123"
        assert item.author_name == "测试用户"
        assert item.content_text == "正文内容"
        assert item.content_media == ["img1.jpg", "img2.jpg"]
        assert item.url == "https://www.xiaohongshu.com/explore/N001"
        assert item.published_at == "2026-04-25T10:00:00Z"
        assert item.metrics == {"likes": 1234, "comments": 56, "shares": 78, "collects": 90}
        assert item.fetched_at  # auto-filled

    def test_extract_minimal(self):
        adapter = RedNoteAdapter.__new__(RedNoteAdapter)
        adapter.platform_name = "rednote"
        item = adapter.extract({"item_id": "X", "content": "hello"})
        assert item.item_id == "X"
        assert item.content_text == "hello"
        assert item.platform == "rednote"
        assert item.author_name == ""

    def test_extract_prefers_alternate_keys(self):
        adapter = RedNoteAdapter.__new__(RedNoteAdapter)
        adapter.platform_name = "rednote"
        item = adapter.extract({
            "id": "alt_id",
            "username": "alt_user",
            "title": "alt_title",
            "media": ["img.png"],
        })
        assert item.item_id == "alt_id"
        assert item.author_name == "alt_user"
        assert item.content_text == "alt_title"

    # ── health_check ──

    @pytest.mark.asyncio
    async def test_health_check_ok(self):
        adapter, browser = await self._make_adapter()
        url = self._write_page("health_check.html", _health_check_html())
        adapter._base_url = url
        try:
            ok = await adapter.health_check()
            assert ok is True
        finally:
            await browser.stop()

    @pytest.mark.asyncio
    async def test_health_check_fail(self):
        adapter, browser = await self._make_adapter(
            base_url="http://127.0.0.1:1"
        )
        try:
            ok = await adapter.health_check()
            assert ok is False
        finally:
            await browser.stop()

    # ── search / extraction ──

    @pytest.mark.asyncio
    async def test_search_extracts_all_items(self):
        html = _build_test_html(n=12, start_hidden=5, batch=5)
        url = self._write_page("rednote_search.html", html)

        adapter, browser = await self._make_adapter(search_url=url)
        try:
            results = []
            async for item in adapter.search(["测试"], limit=20):
                results.append(item)

            assert len(results) == 12
            assert all(isinstance(i, UnifiedItem) for i in results)
            assert results[0].item_id == "1"
            assert results[0].author_name == "用户1"
            assert results[0].content_text == "这是第1条笔记的正文内容。AI技术正在快速发展。"
            assert results[0].metrics["likes"] == 10
            assert results[-1].item_id == "12"
            assert results[-1].metrics["likes"] == 120
            assert results[0].platform == "rednote"
        finally:
            await browser.stop()

    @pytest.mark.asyncio
    async def test_search_respects_limit(self):
        html = _build_test_html(n=15, start_hidden=5, batch=5)
        url = self._write_page("search_limit.html", html)

        adapter, browser = await self._make_adapter(search_url=url)
        try:
            results = []
            async for item in adapter.search(["test"], limit=3):
                results.append(item)

            assert len(results) == 3
        finally:
            await browser.stop()

    @pytest.mark.asyncio
    async def test_search_empty_page(self):
        url = self._write_page("search_empty.html", _empty_page_html())

        adapter, browser = await self._make_adapter(search_url=url)
        try:
            results = []
            async for item in adapter.search(["nothing"], limit=10):
                results.append(item)

            assert len(results) == 0
        finally:
            await browser.stop()

    @pytest.mark.asyncio
    async def test_search_no_duplicates(self):
        html = _build_test_html(n=8, start_hidden=8, batch=8)
        url = self._write_page("search_nodup.html", html)

        adapter, browser = await self._make_adapter(
            search_url=url, max_scrolls=10, scroll_delay=0.05
        )
        try:
            ids = []
            async for item in adapter.search(["test"], limit=20):
                ids.append(item.item_id)

            assert len(ids) == len(set(ids)), f"duplicates found: {ids}"
            assert len(ids) == 8
        finally:
            await browser.stop()

    @pytest.mark.asyncio
    async def test_search_with_scroll_lazy_loading(self):
        html = _build_test_html(n=15, start_hidden=3, batch=4)
        url = self._write_page("search_scroll.html", html)

        adapter, browser = await self._make_adapter(
            search_url=url, max_scrolls=10, scroll_delay=0.05
        )
        try:
            ids = []
            async for item in adapter.search(["scroll test"], limit=20):
                ids.append(item.item_id)

            assert len(ids) == 15
            assert ids == [str(i) for i in range(1, 16)]
        finally:
            await browser.stop()

    # ── get_user_posts ──

    @pytest.mark.asyncio
    async def test_get_user_posts(self):
        html = _build_test_html(n=5, start_hidden=5, batch=5)
        url = self._write_page("user_profile.html", html)

        adapter, browser = await self._make_adapter(
            user_url_template=f"{url}?u={{user_id}}"
        )
        try:
            results = []
            async for item in adapter.get_user_posts("profile123", limit=10):
                results.append(item)

            assert len(results) == 5
        finally:
            await browser.stop()

    # ── get_trending ──

    @pytest.mark.asyncio
    async def test_get_trending(self):
        html = _build_test_html(n=4, start_hidden=4, batch=4)
        url = self._write_page("trending.html", html)

        adapter, browser = await self._make_adapter(trending_url=url)
        try:
            results = []
            async for item in adapter.get_trending(limit=10):
                results.append(item)

            assert len(results) == 4
        finally:
            await browser.stop()

    # ── stealth injection ──

    @pytest.mark.asyncio
    async def test_stealth_injected(self):
        html = _build_test_html(n=1, start_hidden=1, batch=1)
        url = self._write_page("stealth_test.html", html)

        adapter, browser = await self._make_adapter(search_url=url)
        try:
            results = []
            async for item in adapter.search(["stealth"], limit=1):
                results.append(item)

            assert len(results) == 1
        finally:
            await browser.stop()

    # ── _parse_int edge cases ──

    def test_parse_int_wan(self):
        assert RedNoteAdapter._parse_int("1.2万") == 12000

    def test_parse_int_wan_invalid(self):
        assert RedNoteAdapter._parse_int("abc万") == 0

    def test_parse_int_k(self):
        assert RedNoteAdapter._parse_int("3.5k") == 3500

    def test_parse_int_k_invalid(self):
        assert RedNoteAdapter._parse_int("xyzk") == 0

    def test_parse_int_comma(self):
        assert RedNoteAdapter._parse_int("1,234") == 1234

    def test_parse_int_invalid(self):
        assert RedNoteAdapter._parse_int("not a number") == 0

    def test_parse_int_empty(self):
        assert RedNoteAdapter._parse_int("") == 0

    # ── scrape with missing elements ──

    @pytest.mark.asyncio
    async def test_search_sparse_item_still_extracted(self):
        html = _build_test_html(n=3, start_hidden=3, batch=3)
        url = self._write_page("search_sparse.html", html)

        adapter, browser = await self._make_adapter(search_url=url)
        try:
            results = []
            async for item in adapter.search(["test"], limit=10):
                results.append(item)

            assert len(results) == 3
            assert results[1].item_id == "2"
            assert results[1].content_text == "这是第2条笔记的正文内容。AI技术正在快速发展。"
        finally:
            await browser.stop()

    def test_parse_int_negative(self):
        assert RedNoteAdapter._parse_int("-1") == -1

    @pytest.mark.asyncio
    async def test_search_no_data_id_falls_back_to_title(self):
        html = _build_test_html(n=1, start_hidden=1, batch=1)
        url = self._write_page("search_noid.html",
                               html.replace('data-id="1"', ""))

        adapter, browser = await self._make_adapter(search_url=url)
        try:
            results = []
            async for item in adapter.search(["test"], limit=10):
                results.append(item)
            assert len(results) == 1
            assert results[0].item_id == "帖子标题 #1"
        finally:
            await browser.stop()
