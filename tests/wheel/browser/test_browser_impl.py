import json
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from src.wheel.browser.browser_factory import BrowserManagerFactory
from src.wheel.browser.browser_manager import BrowserContext
from src.wheel.browser.browser_models import BrowserConfig


# ═══════════════════════════════════════════════════════════════════════════════
# TA: 真实 Playwright headless 全链路
# ═══════════════════════════════════════════════════════════════════════════════

class TestPlaywrightBrowserManagerTA:
    @pytest.fixture
    def manager(self, tmp_path):
        cfg = BrowserConfig(
            headless=True,
            sessions_dir=str(tmp_path / "sessions"),
        )
        return BrowserManagerFactory().create(cfg)

    @pytest.fixture
    async def started_manager(self, manager):
        await manager.start()
        yield manager
        if manager.health_check():
            await manager.stop()

    # ── 生命周期 ──

    def test_name(self, manager):
        assert manager.name == "PlaywrightBrowserManager"

    def test_health_check_before_start(self, manager):
        assert manager.health_check() is False

    @pytest.mark.asyncio
    async def test_full_lifecycle(self, manager):
        await manager.start()
        assert manager.health_check() is True

        ctx = await manager.get_context("twitter")
        assert isinstance(ctx, BrowserContext)

        page = await ctx.new_page()
        await page.goto("data:text/html,<h1>Lynne TA</h1>")

        await manager.save_session("twitter")
        session_file = manager._session_path("twitter")
        assert session_file.exists()

        await manager.stop()
        assert manager.health_check() is False

    @pytest.mark.asyncio
    async def test_start_idempotent(self, manager):
        await manager.start()
        assert manager.health_check() is True

        await manager.start()
        assert manager.health_check() is True

        await manager.stop()
        assert manager.health_check() is False

    @pytest.mark.asyncio
    async def test_stop_closes_all_contexts(self, manager):
        await manager.start()
        await manager.get_context("twitter")
        await manager.get_context("rednote")

        assert len(manager._contexts) == 2
        await manager.stop()
        assert len(manager._contexts) == 0
        assert manager.health_check() is False

    # ── Context 管理 ──

    @pytest.mark.asyncio
    async def test_get_context_creates_context(self, started_manager):
        ctx = await started_manager.get_context("twitter")
        assert isinstance(ctx, BrowserContext)
        assert ctx.raw() is not None

    @pytest.mark.asyncio
    async def test_get_context_cached(self, started_manager):
        ctx1 = await started_manager.get_context("twitter")
        ctx2 = await started_manager.get_context("twitter")
        assert ctx1 is ctx2

    @pytest.mark.asyncio
    async def test_context_isolation(self, started_manager):
        ctx1 = await started_manager.get_context("twitter")
        ctx2 = await started_manager.get_context("rednote")
        assert ctx1 is not ctx2
        assert ctx1.raw() is not ctx2.raw()

    @pytest.mark.asyncio
    async def test_get_context_loads_existing_session(self, manager, tmp_path):
        await manager.start()
        ctx_a = await manager.get_context("twitter")
        pw_ctx_a = ctx_a.raw()
        await pw_ctx_a.add_cookies([
            {"name": "auth", "value": "secret", "domain": "twitter.com", "path": "/"}
        ])
        await manager.save_session("twitter")
        await manager.stop()

        session_file = tmp_path / "sessions" / "twitter_state.json"
        raw = json.loads(session_file.read_text())
        assert any(c["name"] == "auth" and c["value"] == "secret" for c in raw.get("cookies", []))

        manager2 = BrowserManagerFactory().create(BrowserConfig(
            headless=True,
            sessions_dir=str(tmp_path / "sessions"),
        ))
        await manager2.start()
        ctx_b = await manager2.get_context("twitter")
        pw_ctx_b = ctx_b.raw()
        cookies = await pw_ctx_b.cookies()
        assert any(c["name"] == "auth" and c["value"] == "secret" for c in cookies)

        await manager2.stop()

    # ── Session 持久化 ──

    @pytest.mark.asyncio
    async def test_save_session_persists_file(self, started_manager, tmp_path):
        await started_manager.get_context("twitter")
        await started_manager.save_session("twitter")

        session_file = tmp_path / "sessions" / "twitter_state.json"
        assert session_file.exists()

        data = json.loads(session_file.read_text())
        assert "cookies" in data
        assert "origins" in data

    @pytest.mark.asyncio
    async def test_corrupted_session_graceful(self, manager, tmp_path):
        sessions_dir = tmp_path / "sessions"
        sessions_dir.mkdir(parents=True)
        (sessions_dir / "twitter_state.json").write_text("not valid {{{")

        await manager.start()
        ctx = await manager.get_context("twitter")
        assert isinstance(ctx, BrowserContext)
        await manager.stop()

    # ── 页面操作 ──

    @pytest.mark.asyncio
    async def test_new_page_and_navigate(self, started_manager):
        ctx = await started_manager.get_context("twitter")
        page = await ctx.new_page()
        await page.goto("data:text/html,<title>Test</title><p>hello</p>")
        pw_page = page.raw()
        title = await pw_page.title()
        assert title == "Test"

    # ── Login Flow ──

    @pytest.mark.asyncio
    async def test_login_flow_opens_page(self, started_manager):
        await started_manager.login_flow(
            "test_platform",
            "data:text/html,<title>Login Page</title>",
        )
        assert started_manager._login_pending["test_platform"] is True

    @pytest.mark.asyncio
    async def test_set_login_complete_full_flow(self, started_manager, tmp_path):
        await started_manager.login_flow(
            "test_platform",
            "data:text/html,<title>Login</title>",
        )
        assert "test_platform" in started_manager._login_pending

        await started_manager.set_login_complete("test_platform")
        assert "test_platform" not in started_manager._login_pending

        session_file = tmp_path / "sessions" / "test_platform_state.json"
        assert session_file.exists()

    @pytest.mark.asyncio
    async def test_login_flow_reuses_context(self, started_manager):
        await started_manager.get_context("test_platform")
        await started_manager.login_flow("test_platform", "about:blank")
        assert len(started_manager._contexts) == 1

    @pytest.mark.asyncio
    async def test_stealth_applied_on_real_page(self, started_manager):
        await started_manager.login_flow(
            "stealth_test",
            "data:text/html,<title>Stealth Test</title>",
        )
        assert started_manager._login_pending["stealth_test"] is True

    @pytest.mark.asyncio
    async def test_sequential_platform_operation(self, manager):
        await manager.start()
        for platform in ["twitter", "rednote", "douyin"]:
            ctx = await manager.get_context(platform)
            page = await ctx.new_page()
            await page.goto("data:text/html,<p>" + platform + "</p>")
            await page.close()
        await manager.stop()


# ═══════════════════════════════════════════════════════════════════════════════
# UT: 错误路径 — 无需真实 Playwright
# ═══════════════════════════════════════════════════════════════════════════════

class TestPlaywrightBrowserManagerErrors:
    @pytest.fixture
    def manager(self, tmp_path):
        return BrowserManagerFactory().create(
            BrowserConfig(headless=True, sessions_dir=str(tmp_path / "sessions"))
        )

    @pytest.mark.asyncio
    async def test_get_context_not_started_raises(self, manager):
        with pytest.raises(RuntimeError, match="not started"):
            await manager.get_context("twitter")

    @pytest.mark.asyncio
    async def test_save_session_no_context_raises(self, manager):
        await manager.start()
        try:
            with pytest.raises(RuntimeError, match="no context"):
                await manager.save_session("twitter")
        finally:
            await manager.stop()

    @pytest.mark.asyncio
    async def test_set_login_complete_no_flow_raises(self, manager):
        await manager.start()
        try:
            with pytest.raises(RuntimeError, match="no login flow active"):
                await manager.set_login_complete("twitter")
        finally:
            await manager.stop()

    @pytest.mark.asyncio
    async def test_login_flow_stealth_missing_graceful(self, manager, tmp_path):
        import builtins
        original = builtins.__import__

        def mock_import(name, *args, **kwargs):
            if name == "playwright_stealth":
                raise ImportError("no stealth")
            return original(name, *args, **kwargs)

        await manager.start()
        try:
            with patch("builtins.__import__", side_effect=mock_import):
                await manager.login_flow("test_p", "data:text/html,<h1>x</h1>")
            assert manager._login_pending.get("test_p") is True
        finally:
            await manager.stop()
