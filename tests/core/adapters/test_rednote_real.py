"""
TA: RedNoteAdapter — real Xiaohongshu.com E2E tests.

Single browser for both login and search/trending. No stealth, no session reload.
"""
import asyncio
import os

import pytest
import yaml

from src.core.adapters.adapter_models import AdapterConfig
from src.core.adapters.imp.rednote_adapter import RedNoteAdapter
from src.common.models import UnifiedItem
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


_ACCOUNT = _load_rednote_account()
_HAS_CREDS = _ACCOUNT is not None
_SKIP_REASON = "RedNote credentials not configured"


@pytest.mark.skipif(not _HAS_CREDS, reason=_SKIP_REASON)
class TestRedNoteRealTA:
    @pytest.fixture(autouse=True, scope="class")
    async def _class_setup(self, tmp_path_factory):
        tmp_path = tmp_path_factory.mktemp("rednote_real")
        sessions_dir = str(tmp_path / "sessions")

        browser = BrowserManagerFactory().create(BrowserConfig(
            headless=False, sessions_dir=sessions_dir,
        ))
        await browser.start()

        adapter = RedNoteAdapter(
            browser,
            AdapterConfig(platform="rednote", scroll_delay=1.5, max_scrolls=10),
        )

        await adapter.login(_ACCOUNT)

        TestRedNoteRealTA._browser = browser
        TestRedNoteRealTA._adapter = adapter
        yield
        await browser.stop()

    @staticmethod
    async def _collect(gen, timeout: float) -> list[UnifiedItem]:
        items: list[UnifiedItem] = []
        deadline = asyncio.get_event_loop().time() + timeout
        async for item in gen:
            items.append(item)
            if asyncio.get_event_loop().time() > deadline:
                break
        return items

    async def test_search_real(self):
        print("\n  searching '防晒' ...")
        try:
            items = await asyncio.wait_for(
                self._collect(self._adapter.search(["防晒"], limit=5), 60),
                timeout=90,
            )
        except asyncio.TimeoutError:
            pytest.fail("search: outer timeout (90s)")
        except Exception as e:
            pytest.fail(f"search failed: {e}")

        for item in items:
            print(f"    id={item.item_id} title={item.content_text[:50]}")
        assert len(items) > 0, "search returned 0 items"
        for item in items:
            assert isinstance(item, UnifiedItem)
            assert item.platform == "rednote"
            assert item.item_id
        print(f"\n  search OK: {len(items)} items")

    async def test_get_trending_real(self):
        print("\n  fetching trending ...")
        try:
            items = await asyncio.wait_for(
                self._collect(self._adapter.get_trending(limit=5), 60),
                timeout=90,
            )
        except asyncio.TimeoutError:
            pytest.fail("trending: outer timeout (90s)")
        except Exception as e:
            pytest.fail(f"trending failed: {e}")

        assert len(items) > 0, "trending returned 0 items"
        for item in items:
            assert isinstance(item, UnifiedItem)
            assert item.platform == "rednote"
            assert item.item_id
        print(f"\n  trending OK: {len(items)} items")
