"""
CDP 可用性诊断。
Phase 1-5: 本地模拟重 JS 页面
Phase 6: 真实 RedNote 登录后页面
"""
import asyncio
import os
import time

import pytest
import yaml

from src.core.adapters.adapter_models import AdapterConfig
from src.core.adapters.imp.rednote_adapter import RedNoteAdapter
from src.wheel.browser.browser_factory import BrowserManagerFactory
from src.wheel.browser.browser_models import BrowserConfig


def _load_rednote_account() -> dict[str, str] | None:
    config_path = os.path.normpath(
        os.path.join(os.path.dirname(__file__), "..", "..", "..", "config.yaml")
    )
    if not os.path.exists(config_path):
        return None
    with open(config_path, "r", encoding="utf-8") as f:
        raw = yaml.safe_load(f)
    platforms = raw.get("platforms", {})
    rednote = platforms.get("rednote", {})
    account = rednote.get("account", {})
    phone = account.get("phone", "")
    password = account.get("password", "")
    if phone.startswith("${") and phone.endswith("}"):
        var_name = phone[2:-1]
        phone = os.environ.get(var_name, "")
    if not phone:
        return None
    return {"phone": phone, "password": password}


_HEAVY_JS_PAGE = """<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Heavy SPA Test</title>
<style>
  body { font-family: sans-serif; padding: 20px; }
  #feed div { border: 1px solid #ddd; margin: 8px 0; padding: 12px; }
  .title { font-size: 16px; font-weight: bold; }
  .author .name { color: #999; font-size: 13px; }
  .like-wrapper .count { color: #ff4d4f; }
  .cover img { width: 120px; height: 120px; }
</style></head><body>
<h1>SPA 模拟页面</h1>
<input class="search-input" placeholder="搜索笔记" />
<div id="feed">
{items}
</div>
<script>
// ── 模拟 Service Worker ──
if ('serviceWorker' in navigator) {{
  // 注册请求不实际发出，但有异步开销
}}

// ── 模拟埋点 XHR 轮询 ──
function fakeBeacon() {{
  fetch('/noop').catch(function(){{}});
}}
setInterval(fakeBeacon, 200);

// ── CPU 忙循环（模拟 Vue hydration） ──
let counter = 0;
setInterval(function() {{
  let x = 0;
  for (let i = 0; i < 100000; i++) x += Math.sqrt(i);
  document.title = 'Heavy SPA [' + (++counter) + ']';
}}, 500);

// ── 连续 DOM 写 ──
setInterval(function() {{
  let el = document.createElement('span');
  el.textContent = '.';
  el.style.display = 'none';
  document.body.appendChild(el);
  if (document.body.children.length > 1000) {{
    document.body.removeChild(el);
  }}
}}, 100);
</script></body></html>"""


def _build_page(n: int = 20):
    items = []
    for i in range(1, n + 1):
        items.append(f"""\
<div class="note-item" data-id="{i}">
  <a class="cover" href="/explore/item_{i}">
    <img src="data:image/svg+xml,<svg/>" />
  </a>
  <div class="title"><span>帖子标题 #{i}</span></div>
  <div class="author"><span class="name">用户{i}</span></div>
  <div class="desc">这是第{i}条笔记的正文内容。AI技术正在快速发展。</div>
  <div class="like-wrapper"><span class="count">{i * 10}</span></div>
</div>""")
    return _HEAVY_JS_PAGE.replace("{items}", "\n".join(items))


_H = "─"


class TestCdpDiagnostics:
    @pytest.fixture(autouse=True)
    def _setup(self, tmp_path):
        self._tmp_path = tmp_path
        browser_cfg = BrowserConfig(
            headless=True,
            sessions_dir=str(tmp_path / "sessions"),
        )
        self._browser = BrowserManagerFactory().create(browser_cfg)

    async def _init(self):
        await self._browser.start()
        ctx = await self._browser.get_context("test")
        page = await ctx.new_page()
        return ctx, page

    def _write_page(self, html: str) -> str:
        p = self._tmp_path / "test.html"
        p.write_text(html, encoding="utf-8")
        return f"file://{p}"

    async def _timed(self, label: str, coro, timeout: float = 5.0):
        """Run coro with timeout, return (label, ok, elapsed_ms, error)."""
        t0 = time.monotonic()
        try:
            await asyncio.wait_for(coro, timeout=timeout)
            elapsed = (time.monotonic() - t0) * 1000
            return (label, True, f"{elapsed:.0f}ms", None)
        except asyncio.TimeoutError:
            elapsed = (time.monotonic() - t0) * 1000
            return (label, False, f"{elapsed:.0f}ms", "timeout")
        except Exception as e:
            elapsed = (time.monotonic() - t0) * 1000
            return (label, False, f"{elapsed:.0f}ms", str(e)[:80])

    # ═══════════════════════════════════════════
    # Phase 1: 无负载 — 基线
    # ═══════════════════════════════════════════

    @pytest.mark.asyncio
    async def test_phase1_baseline_no_load(self, capsys):
        ctx, page = await self._init()
        pw = page.raw()
        url = self._write_page(_build_page(10))
        try:
            results = []

            results.append(await self._timed("page.goto(load)", page.goto(url, wait_until="load")))
            await asyncio.sleep(1)

            results.append(await self._timed("pw.title()", pw.title()))
            results.append(await self._timed("pw.evaluate(simple)", pw.evaluate("1+1")))
            results.append(await self._timed("pw.inner_text(body)", pw.inner_text("body")))
            results.append(await self._timed("pw.query_selector(.title)",
                                             pw.query_selector(".title span")))
            results.append(await self._timed("pw.query_selector_all(.note-item)",
                                             pw.query_selector_all(".note-item")))
            results.append(await self._timed("el.get_attribute(data-id)",
                                             (await pw.query_selector("[data-id]")).get_attribute("data-id")))
            results.append(await self._timed("pw.content()", pw.content()))
            results.append(await self._timed("ctx.new_page()", ctx.new_page()))
            results.append(await self._timed("page.goto(about:blank)", page.goto("about:blank")))
            results.append(await self._timed("page.close()", page.close()))

            self._print_report("Phase 1: 无 JS 负载基线", results, capsys)
            assert all(r[1] for r in results), f"基线阶段有 CDP 操作失败: {[r for r in results if not r[1]]}"
        finally:
            await self._browser.stop()

    # ═══════════════════════════════════════════
    # Phase 2: 重 JS 负载 — 模拟 SPA
    # ═══════════════════════════════════════════

    @pytest.mark.asyncio
    async def test_phase2_heavy_js_load(self, capsys):
        ctx, page = await self._init()
        pw = page.raw()
        url = self._write_page(_build_page(20))
        try:
            # Load the heavy JS page
            await page.goto(url, wait_until="load")
            # Wait for the JS to start running
            await asyncio.sleep(3)
            print("\n  [页面加载完成，重 JS 已运行 3s，开始 CDP 诊断...]", file=None)

            results = []

            results.append(await self._timed("pw.title()", pw.title()))
            results.append(await self._timed("pw.query_selector(.title)",
                                             pw.query_selector(".title span")))
            results.append(await self._timed("pw.query_selector_all(.note-item)",
                                             pw.query_selector_all(".note-item")))
            results.append(await self._timed("pw.evaluate(1+1)", pw.evaluate("1+1")))
            results.append(await self._timed("pw.inner_text(body)", pw.inner_text("body")))
            results.append(await self._timed("pw.content()", pw.content()))
            results.append(await self._timed("ctx.new_page()", ctx.new_page(), timeout=3.0))
            results.append(await self._timed("page.goto(about:blank)",
                                             page.goto("about:blank"), timeout=3.0))

            self._print_report("Phase 2: 重 JS 负载", results, capsys)

            # 这种重负载下 query_selector 可能超时，不强制 assert pass
            failed = [r for r in results if not r[1]]
            if failed:
                print(f"\n  [WARNING] {len(failed)} 个 CDP 操作超时: "
                      f"{', '.join(r[0] for r in failed)}")
        finally:
            await self._browser.stop()

    # ═══════════════════════════════════════════
    # Phase 3: 逐步衰减 — 定位第几秒开始挂
    # ═══════════════════════════════════════════

    @pytest.mark.asyncio
    async def test_phase3_decay_timeline(self, capsys):
        ctx, page = await self._init()
        pw = page.raw()
        url = self._write_page(_build_page(10))
        try:
            await page.goto(url, wait_until="load")
            print(f"\n  {'秒数':>5} | {'操作':<30} | {'结果':>6} | {'耗时':>8} | 详情")

            for sec in [0, 2, 5, 10, 15, 20, 30]:
                await asyncio.sleep(max(0, sec - (time.monotonic() - self._t0))
                                    if hasattr(self, '_t0') else 0)
                if sec == 0:
                    self._t0 = time.monotonic()

                results = [
                    await self._timed("pw.title()", pw.title(), timeout=3),
                    await self._timed("pw.query_selector(.title)", pw.query_selector(".title span"), timeout=3),
                    await self._timed("pw.evaluate(1+1)", pw.evaluate("1+1"), timeout=3),
                ]
                for label, ok, elapsed, err in results:
                    status = "  OK  " if ok else "FAILED"
                    detail = "" if ok else f" ({err})"
                    print(f"  {sec:>5}s | {label:<30} | {status:>6} | {elapsed:>8} |{detail}")

            print()
        finally:
            await self._browser.stop()

    # ═══════════════════════════════════════════
    # Phase 4: wait_until 四种策略对比
    # ═══════════════════════════════════════════

    @pytest.mark.asyncio
    async def test_phase4_wait_until_strategies(self, capsys):
        ctx, page = await self._init()
        url = self._write_page(_build_page(10))
        strategies = ["commit", "domcontentloaded", "load", "networkidle"]
        results = []
        try:
            for strategy in strategies:
                p = await ctx.new_page()
                label = f"page.goto({strategy})"
                results.append(
                    await self._timed(label, p.goto(url, wait_until=strategy, timeout=15000), timeout=20)
                )
                await p.close()

            self._print_report("Phase 4: wait_until 策略对比", results, capsys)
        finally:
            await self._browser.stop()

    # ═══════════════════════════════════════════
    # Phase 5: JS 禁用 — 对比
    # ═══════════════════════════════════════════

    @pytest.mark.asyncio
    async def test_phase5_js_disabled_vs_enabled(self, capsys):
        ctx, page = await self._init()
        pw_ctx = ctx.raw()
        pw_browser = pw_ctx.browser
        page_url = self._write_page(_build_page(20))

        scenarios = [
            ("JS 启用",  None),
            ("JS 禁用",  {"java_script_enabled": False}),
        ]

        try:
            for label, kw in scenarios:
                results = []
                if kw:
                    new_pw_ctx = await pw_browser.new_context(**kw)
                else:
                    new_pw_ctx = await pw_browser.new_context()
                p = await new_pw_ctx.new_page()

                results.append(await self._timed(f"[{label}] page.goto(load)",
                                                 p.goto(page_url, wait_until="load"), timeout=15))
                await asyncio.sleep(3)
                results.append(await self._timed(f"[{label}] pw.title()", p.title()))
                results.append(await self._timed(f"[{label}] pw.query_selector(.title)",
                                                 p.query_selector(".title span")))
                results.append(await self._timed(f"[{label}] pw.query_selector_all(.note-item)",
                                                 p.query_selector_all(".note-item")))
                results.append(await self._timed(f"[{label}] pw.evaluate(1+1)",
                                                 p.evaluate("1+1")))
                results.append(await self._timed(f"[{label}] pw.content()", p.content()))

                self._print_report(f"Phase 5: {label}", results, capsys)
                await p.close()
                if kw:
                    await new_pw_ctx.close()
        finally:
            await self._browser.stop()

    # ═══════════════════════════════════════════
    # Phase 6: 真实 RedNote 登录后 CDP 诊断
    # ═══════════════════════════════════════════

    @pytest.mark.asyncio
    async def test_phase6_real_rednote_after_login(self, capsys):
        account = _load_rednote_account()
        if not account:
            pytest.skip("RedNote credentials not configured")

        browser = BrowserManagerFactory().create(BrowserConfig(
            headless=False,
            sessions_dir=str(self._tmp_path / "sessions"),
        ))
        await browser.start()

        try:
            adapter = RedNoteAdapter(
                browser,
                AdapterConfig(platform="rednote", scroll_delay=1.5, max_scrolls=10),
            )
            ok = await adapter.login(account)
            if not ok:
                pytest.skip("login failed")

            # Get the logged-in page (at /explore)
            ctx = await browser.get_context("rednote")
            page = ctx.pages[-1]
            pw = page.raw()
            print(f"\n  [已登录，当前页: {pw.url}]")

            # Short sleep to let SPA initialize
            await asyncio.sleep(2)

            results = []
            # ══ page-level read ops ══
            results.append(await self._timed("pw.url", self._url_coro(pw), timeout=2))
            results.append(await self._timed("pw.title()", pw.title(), timeout=5))
            results.append(await self._timed("pw.content()", pw.content(), timeout=10))
            results.append(await self._timed("pw.evaluate(1+1)", pw.evaluate("1+1"), timeout=5))
            results.append(await self._timed("pw.query_selector(body)", pw.query_selector("body"), timeout=5))
            results.append(await self._timed("pw.query_selector(h1)", pw.query_selector("h1"), timeout=5))
            results.append(await self._timed("pw.query_selector(.note-item)", pw.query_selector(".note-item"), timeout=5))
            results.append(await self._timed("pw.query_selector_all(.note-item)", pw.query_selector_all(".note-item"), timeout=5))
            results.append(await self._timed("pw.inner_text(body)", pw.inner_text("body"), timeout=5))

            # ══ ctx-level ops ══
            results.append(await self._timed("ctx.new_page()", ctx.new_page(), timeout=5))

            # ══ page goto ops ══
            results.append(await self._timed("page.goto(about:blank)", page.goto("about:blank"), timeout=10))
            results.append(await self._timed("page.goto(/explore)", page.goto("https://www.xiaohongshu.com/explore"), timeout=3))
            results.append(await self._timed("page.goto(/login)", page.goto("https://www.xiaohongshu.com/login"), timeout=15))
            results.append(await self._timed("page.goto(search_url)", page.goto("https://www.xiaohongshu.com/search_result?keyword=测试"), timeout=15))

            self._print_report("Phase 6: 真实 RedNote 登录后页面", results, capsys)

            ok_count = sum(1 for r in results if r[1])
            total = len(results)
            failed = [r[0] for r in results if not r[1]]
            print(f"\n  CDP 可用: {ok_count}/{total}")
            if failed:
                print(f"  超时/失败: {', '.join(failed)}")
                print("\n  诊断: 这不是 JS 太重，是 RedNote 反爬阻止了 CDP 协议。")
        finally:
            await browser.stop()

    @staticmethod
    async def _url_coro(pw):
        return pw.url

    @staticmethod
    def _log_info(msg, vars, **kw):
        # Simple log helper
        pass

    # ── helpers ──

    def _print_report(self, title, results, capsys):
        print(f"\n  {_H * 50}")
        print(f"  {title}")
        print(f"  {_H * 50}")
        print(f"  {'操作':<35} {'状态':>6} {'耗时':>10}")
        print(f"  {'-'*35} {'-'*6} {'-'*10}")
        for label, ok, elapsed, err in results:
            status = "  OK" if ok else "FAIL"
            detail = f"  → {err}" if err else ""
            print(f"  {label:<35} {status:>6} {elapsed:>10}{detail}")
        print(f"  {_H * 50}")
        ok_count = sum(1 for r in results if r[1])
        print(f"  通过: {ok_count}/{len(results)}")
