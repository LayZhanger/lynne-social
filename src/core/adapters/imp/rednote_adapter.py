import asyncio
from datetime import datetime, timezone
from typing import AsyncIterator
from urllib.parse import quote

from src.common.models import UnifiedItem
from src.wheel.browser.browser_manager import BrowserManager
from src.wheel.logger import get_logger

from .llm_adapter import LLMAdapter
from ..adapter_models import AdapterConfig

# RedNote DOM selectors (CSS fallback)
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
_LOGIN_URL = "https://www.xiaohongshu.com/login"

class RedNoteAdapter(LLMAdapter):
    platform_name = "rednote"

    def __init__(self, browser_manager: BrowserManager, config: AdapterConfig,
                 *,
                 llm_config=None,
                 search_url: str | None = None,
                 user_url_template: str | None = None,
                 trending_url: str | None = None,
                 base_url: str | None = None):
        super().__init__(browser_manager, config, llm_config=llm_config)
        self._log = get_logger("rednote")
        self._search_url = search_url if search_url is not None else _SEARCH_URL
        self._user_url_template = user_url_template if user_url_template is not None else _USER_URL_TEMPLATE
        self._trending_url = trending_url if trending_url is not None else _TRENDING_URL
        self._base_url = base_url if base_url is not None else _BASE_URL

    async def search(self, keywords: list[str], limit: int) -> AsyncIterator[UnifiedItem]:
        self._log.info("search: begin")
        ctx = await self._browser.get_context(self.platform_name)
        keyword = " ".join(keywords)

        if self._search_url != _SEARCH_URL:
            # Custom URL (test mode) — goto
            page = ctx.pages[-1] if ctx.pages else await ctx.new_page()
            url = self._search_url.replace("{keyword}", quote(keyword))
            self._log.info("search: goto {}", url)
            await page.goto(url, wait_until="domcontentloaded", timeout=30000)
            await asyncio.sleep(2)
        else:
            # Real mode — goto search page directly (new page to avoid CDP issues in non-headless)
            page = await ctx.new_page()
            keyword = " ".join(keywords)
            url = f"https://www.xiaohongshu.com/search_result?keyword={quote(keyword)}"
            self._log.info("search: goto {}", url)
            await page.goto(url, wait_until="domcontentloaded", timeout=30000)
            await asyncio.sleep(2)
            self._log.info("search: page url={}", page.raw().url)

        count = 0
        async for item in self._extract(page, limit):
            count += 1
            yield item
        self._log.info("search: done, extracted {} items", count)

    async def get_user_posts(self, user_id: str, limit: int) -> AsyncIterator[UnifiedItem]:
        ctx = await self._browser.get_context(self.platform_name)
        page = ctx.pages[-1] if ctx.pages else await ctx.new_page()
        url = self._user_url_template.replace("{user_id}", quote(user_id))
        await page.goto(url, wait_until="domcontentloaded", timeout=30000)
        await asyncio.sleep(2)
        async for item in self._extract(page, limit):
            yield item

    async def get_trending(self, limit: int) -> AsyncIterator[UnifiedItem]:
        self._log.info("trending: begin")
        ctx = await self._browser.get_context(self.platform_name)

        if self._trending_url != _TRENDING_URL:
            page = ctx.pages[-1] if ctx.pages else await ctx.new_page()
            self._log.info("trending: goto {}", self._trending_url)
            await page.goto(self._trending_url, wait_until="domcontentloaded", timeout=30000)
            await asyncio.sleep(2)
        else:
            if not ctx.pages:
                raise RuntimeError("no active page, login first")
            page = ctx.pages[-1]
            self._log.info("trending: already on {}", page.raw().url)
            await asyncio.sleep(2)

        count = 0
        async for item in self._extract(page, limit):
            count += 1
            yield item
        self._log.info("trending: done, extracted {} items", count)

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
            page = ctx.pages[-1] if ctx.pages else await ctx.new_page()
            await page.goto(self._base_url, wait_until="domcontentloaded", timeout=15000)
            return True
        except (asyncio.TimeoutError, Exception):
            self._log.warning("health check failed for rednote")
            return False

    async def login(self, account: dict[str, str] | None = None) -> bool:
        phone = account.get("phone", "") if account else ""
        password = account.get("password", "") if account else ""
        if not phone:
            self._log.warning("no phone provided for rednote login")
            return False

        ctx = await self._browser.get_context(self.platform_name)
        page = ctx.pages[-1] if ctx.pages else await ctx.new_page()
        pw = page.raw()
        login_ok = False

        await self._inject_stealth(page)
        try:
            await page.goto(_LOGIN_URL, wait_until="domcontentloaded", timeout=20000)
        except Exception as e:
            self._log.error("login page goto failed: {}", e)
            return False
        await asyncio.sleep(3)
        self._log.info("login page loaded, url={}", pw.url)

        # ── Password login (if password provided) ──
        if password:
            try:
                login_ok = await self._try_password_login(pw, phone, password)
            except Exception as e:
                self._log.warning("password login raised: {}", e)

        # ── SMS fallback ──
        if not login_ok:
            login_ok = await self._try_sms_login(pw, phone)

        if login_ok:
            await self._browser.save_session(self.platform_name)
            self._log.info("rednote login completed, session saved")

            page = ctx.pages[-1] if ctx.pages else await ctx.new_page()
            await page.goto(_BASE_URL + "/explore", wait_until="domcontentloaded", timeout=20000)
            await asyncio.sleep(2)
            self._log.info("login: navigated to explore, url={}", page.raw().url)
        else:
            self._log.error("login failed: SMS verification code not entered (no password on web)")
        return login_ok

    async def _try_password_login(self, pw, phone: str, password: str) -> bool:
        """Attempt password-based login. Returns True on success, False to fallback."""
        self._log.info("trying password login...")

        # Switch to password login tab
        pwd_tab = None
        for sel in [
            'span:has-text("密码登录")',
            'div:has-text("密码登录")',
            'text=密码登录',
            'span:has-text("密码")',
            'text=密码',
        ]:
            pwd_tab = await pw.query_selector(sel)
            if pwd_tab:
                break

        if pwd_tab:
            await pwd_tab.click()
            self._log.info("clicked password login tab")
            await asyncio.sleep(1.5)
        else:
            self._log.info("password login tab not found — page has single phone input?")

        # Find phone input
        phone_input = None
        for sel in [
            'input[placeholder*="手机号"]',
            'input[placeholder*="手机"]',
            'input[name*="phone"]',
            'input[name*="mobile"]',
            'input[type="tel"]',
        ]:
            phone_input = await pw.query_selector(sel)
            if phone_input:
                break

        if not phone_input:
            self._log.info("phone input not found in password mode, falling back")
            return False

        await phone_input.click()
        await asyncio.sleep(0.3)
        await pw.keyboard.type(phone, delay=100)
        self._log.info("password mode: filled phone {}****", phone[:3])

        # Find password input (may appear after clicking tab)
        pwd_input = None
        for sel in [
            'input[type="password"]',
            'input[placeholder*="密码"]',
            'input[name*="password"]',
            'input[name*="pwd"]',
            'input[placeholder*="Password"]',
        ]:
            pwd_input = await pw.query_selector(sel)
            if pwd_input:
                break

        if not pwd_input:
            self._log.info("password input not found, dumping page fragment...")
            try:
                html = (await pw.inner_text("body"))[:500]
                self._log.info("page body: {}", html)
            except Exception as e:
                self._log.info("dump failed: {}", e)
            return False

        await pwd_input.click()
        await asyncio.sleep(0.3)
        await pw.keyboard.type(password, delay=100)
        self._log.info("password mode: filled password ({} chars)", len(password))

        # Click login/submit button
        login_btn = None
        for sel in [
            'button:has-text("登录")',
            'span:has-text("登录")',
            'button:has-text("Login")',
            '[type="submit"]',
            'button[class*="login"]',
            'div[class*="login-btn"]',
        ]:
            login_btn = await pw.query_selector(sel)
            if login_btn:
                break

        if login_btn:
            await login_btn.click()
            self._log.info("password login button clicked, waiting for redirect...")
            await self._wait_login_success(pw)
            return True

        self._log.info("login button not found in password mode")
        return False

    async def _try_sms_login(self, pw, phone: str) -> bool:
        # Ensure on phone/SMS tab
        phone_tabs = [
            'span:has-text("手机登录")',
            'div:has-text("手机登录")',
            '[class*="phone-login-tab"]',
        ]
        for sel in phone_tabs:
            try:
                tab = await pw.query_selector(sel)
                if tab:
                    await tab.click()
                    await asyncio.sleep(1)
                    break
            except Exception:
                continue

        phone_sels = [
            'input[placeholder*="手机号"]',
            'input[placeholder*="手机"]',
            'input[type="tel"]',
            'input[name*="phone"]',
        ]
        phone_input = None
        for sel in phone_sels:
            phone_input = await pw.query_selector(sel)
            if phone_input:
                break

        if not phone_input:
            preview = ""
            try:
                preview = (await pw.inner_text("body"))[:300]
            except Exception:
                pass
            self._log.warning("SMS: phone input not found, url={} body={}", pw.url, preview)
            return False

        await phone_input.click()
        await asyncio.sleep(0.3)
        await pw.keyboard.type(phone, delay=100)
        self._log.info("SMS: filled phone {}****", phone[:3])

        send_sels = [
            'button:has-text("获取验证码")',
            'button:has-text("发送验证码")',
            'button:has-text("发送")',
            'span:has-text("获取验证码")',
            '[class*="send-code"]',
        ]
        send_btn = None
        for sel in send_sels:
            send_btn = await pw.query_selector(sel)
            if send_btn:
                break

        if send_btn:
            await send_btn.click()
            self._log.info("SMS: verification code sent, waiting for manual login ({}s)...")
            await self._wait_login_success(pw)
            return True

        self._log.warning("SMS: send code button not found")
        return False

    async def _wait_login_success(self, pw, timeout_sec: int = 180) -> None:
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout_sec
        tick = 0
        while loop.time() < deadline:
            tick += 1
            await asyncio.sleep(1)

            try:
                url = pw.url
            except Exception:
                self._log.debug("pw.url raised, page may be closed")
                continue

            if tick % 5 == 0:
                self._log.info("waiting for login (tick={}, url={})", tick, url)

            if "/login" not in url:
                try:
                    feed = await pw.query_selector(
                        '[class*="feeds"], [class*="explore"], [class*="note-item"], '
                        '[class*="home"], [class*="Home"], [class*="main-page"]'
                    )
                except Exception:
                    feed = None

                if feed:
                    self._log.info("login success: feed detected at {}", url)
                    await asyncio.sleep(2)
                    return

                if any(p in url for p in ["/explore", "/recommend", "/search"]):
                    self._log.info("login success: navigated to {}", url)
                    await asyncio.sleep(2)
                    return
        raise TimeoutError("login did not complete within {} seconds".format(timeout_sec))

    # ── CSS fallback (private) ──

    async def _css_extract(self, page, limit: int) -> AsyncIterator[UnifiedItem]:
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
        self._log.debug("stealth disabled for debug")

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
