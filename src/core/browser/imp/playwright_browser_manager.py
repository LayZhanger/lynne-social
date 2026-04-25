import json
from pathlib import Path

from playwright.async_api import async_playwright

from src.wheel.logger import get_logger

from ..browser_manager import BrowserContext, BrowserManager
from ..browser_models import BrowserConfig
from .playwright_context import PlaywrightContext


class PlaywrightBrowserManager(BrowserManager):
    def __init__(self, config: BrowserConfig | None = None):
        self._config = config or BrowserConfig()
        self._playwright = None
        self._browser = None
        self._contexts: dict[str, BrowserContext] = {}
        self._login_pending: dict[str, bool] = {}
        self._log = get_logger("browser")

    @property
    def name(self) -> str:
        return "PlaywrightBrowserManager"

    async def start(self) -> None:
        if self._playwright is not None:
            return
        self._playwright = await async_playwright().start()
        self._browser = await self._playwright.chromium.launch(
            headless=self._config.headless,
            slow_mo=self._config.slow_mo,
        )
        self._log.info("browser launched (headless={})", self._config.headless)

    async def stop(self) -> None:
        for ctx in self._contexts.values():
            await ctx.close()
        self._contexts.clear()
        if self._browser:
            await self._browser.close()
            self._browser = None
        if self._playwright:
            await self._playwright.stop()
            self._playwright = None
        self._log.info("browser stopped")

    def health_check(self) -> bool:
        return self._browser is not None and self._browser.is_connected()

    async def get_context(self, platform: str) -> BrowserContext:
        if platform in self._contexts:
            return self._contexts[platform]

        if not self._browser:
            raise RuntimeError("browser not started")

        session_path = self._session_path(platform)
        state = None
        if session_path.exists():
            try:
                state = json.loads(session_path.read_text("utf-8"))
                self._log.debug("loaded session for {} from {}", platform, session_path)
            except (json.JSONDecodeError, OSError):
                self._log.warning("failed to load session for {}, creating new context", platform)

        pw_ctx = await self._browser.new_context(
            viewport={"width": self._config.viewport_width, "height": self._config.viewport_height},
            locale=self._config.locale,
            storage_state=state,
        )
        ctx = PlaywrightContext(pw_ctx)
        self._contexts[platform] = ctx
        self._log.debug("created context for {}", platform)
        return ctx

    async def save_session(self, platform: str) -> None:
        ctx = self._contexts.get(platform)
        if not ctx:
            raise RuntimeError(f"no context for platform {platform}")

        state = await ctx.storage_state()
        session_path = self._session_path(platform)
        session_path.parent.mkdir(parents=True, exist_ok=True)
        session_path.write_text(json.dumps(state, ensure_ascii=False, indent=2), "utf-8")
        self._log.info("saved session for {} to {}", platform, session_path)

    async def login_flow(self, platform: str, url: str) -> None:
        ctx = await self.get_context(platform)
        page = await ctx.new_page()
        pw_page = page.raw()

        try:
            from playwright_stealth import Stealth

            await Stealth().apply_stealth_async(pw_page)
        except ImportError:
            self._log.warning("playwright_stealth not available, skipping stealth injection")

        await page.goto(url, wait_until="domcontentloaded")
        self._login_pending[platform] = True
        self._log.info("login flow started for {} at {}", platform, url)

    async def set_login_complete(self, platform: str) -> None:
        if platform not in self._login_pending:
            raise RuntimeError(f"no login flow active for {platform}")

        await self.save_session(platform)
        self._login_pending.pop(platform, None)

        ctx = self._contexts.get(platform)
        if ctx:
            for page in ctx.pages:
                await page.close()

        self._log.info("login complete for {}", platform)

    def _session_path(self, platform: str) -> Path:
        return Path(self._config.sessions_dir) / f"{platform}_state.json"
