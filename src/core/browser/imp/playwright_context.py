from playwright.async_api import BrowserContext as _PWContext
from playwright.async_api import Page as _PWPage

from ..browser_manager import BrowserContext, BrowserPage


class PlaywrightPage(BrowserPage):
    def __init__(self, pw_page: _PWPage):
        self._pw = pw_page

    async def goto(self, url: str, **kwargs) -> None:
        await self._pw.goto(url, **kwargs)

    async def close(self) -> None:
        await self._pw.close()

    def raw(self) -> _PWPage:
        return self._pw


class PlaywrightContext(BrowserContext):
    def __init__(self, pw_ctx: _PWContext):
        self._pw = pw_ctx

    async def new_page(self) -> BrowserPage:
        pw = await self._pw.new_page()
        return PlaywrightPage(pw)

    async def close(self) -> None:
        await self._pw.close()

    async def storage_state(self) -> dict:
        return await self._pw.storage_state()

    @property
    def pages(self) -> list[BrowserPage]:
        return [PlaywrightPage(p) for p in self._pw.pages]

    def raw(self) -> _PWContext:
        return self._pw
