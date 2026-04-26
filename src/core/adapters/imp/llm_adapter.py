import asyncio
import json
from abc import ABC, abstractmethod
from typing import AsyncIterator
from urllib.parse import urlparse

from src.common.models import UnifiedItem
from src.wheel.logger import get_logger

from ..base_adapter import BaseAdapter

_SKELETON_JS = """() => {
  const result = [];
  const candidates = Array.from(document.querySelectorAll('*')).filter(el => {
    if (el.children.length < 2) return false;
    const txt = (el.textContent || '').trim();
    return txt.length >= 50 && txt.length <= 2000
      && el.querySelector('a')
      && (el.querySelector('img') || txt.length > 100);
  });

  for (const el of candidates.slice(0, 3)) {
    const item = {
      tag: el.tagName.toLowerCase(),
      cls: (typeof el.className === 'string'
        ? el.className.split(' ').filter(c => c.length < 40).join(' ')
        : ''),
      children: Array.from(el.children).slice(0, 15).map(child => {
        const childCls = typeof child.className === 'string'
          ? child.className.split(' ').filter(c => c.length < 40).join(' ')
          : '';
        const a = child.querySelector('a');
        const img = child.querySelector('img');
        return {
          tag: child.tagName.toLowerCase(),
          cls: childCls,
          txt: (child.textContent || '').trim().substring(0, 120),
          ...(a ? {href: a.getAttribute('href') || ''} : {}),
          ...(img ? {src: img.getAttribute('src') || ''} : {}),
        };
      }).filter(c => c.txt.length > 2 || c.href || c.src),
    };
    if (item.children.length >= 2) result.push(item);
  }
  return result;
}"""

_EXTRACT_PROMPT = """你是一个浏览器自动化专家。根据以下网页元素骨架，生成一个 JavaScript 函数 extractPosts()。

要求：
1. 函数名必须为 extractPosts，无参数，返回对象数组
2. 遍历 DOM 提取每个帖子的字段，映射到以下 key：
   - item_id: 帖子的唯一标识（优先取 data-id 属性，否则用 title 文本）
   - title: 帖子标题
   - author_name: 作者名称
   - content: 帖子正文
   - likes: 点赞数（纯数字字符串，去掉逗号、万、k 等单位）
   - url: 帖子链接（如果是相对路径，补全为 https:// 开头的绝对 URL）
   - images: 图片 URL 的字符串数组
3. 所有字段值必须是字符串（images 是字符串数组），null/undefined 用空字符串代替
4. 返回纯 JavaScript 代码，不含 ``` 标记，不含注释

网页元素骨架：
"""


class LLMAdapter(BaseAdapter, ABC):
    """所有需要 LLM 驱动内容提取的平台适配器的中间父类。

    继承 BaseAdapter，封装：
      - 骨架提取 JS
      - LLM 生成 extractPosts() 函数
      - 提取函数缓存（按 URL host + path）
      - LLMEngine 懒初始化（从传入的 LLMConfig 创建）

    子类（RedNoteAdapter 等）必须实现 _css_extract() 作为 CSS 回退路径。
    """

    def __init__(self, browser, config, *,
                 llm_config=None, **url_kwargs):
        self._browser = browser
        self._config = config
        self._llm_config = llm_config
        self._llm = None
        self._extract_fn_cache: dict[str, str] = {}
        self._log = get_logger("llm_adapter")

    @abstractmethod
    async def _css_extract(
        self, page, limit: int
    ) -> AsyncIterator[UnifiedItem]:
        """CSS 选择器回退路径。子类必须实现。"""
        ...

    async def _extract(
        self, page, limit: int
    ) -> AsyncIterator[UnifiedItem]:
        """优先 LLM 路径，不可用时自动 fallback 到 CSS。"""
        extract_fn = await self._get_or_generate_extract_fn(page)
        if extract_fn is not None:
            async for item in self._llm_extract(page, extract_fn, limit):
                yield item
        else:
            async for item in self._css_extract(page, limit):
                yield item

    async def _ensure_llm(self):
        if self._llm is None and self._llm_config is not None:
            from src.wheel.llm.llm_factory import LLMEngineFactory
            self._llm = LLMEngineFactory().create(self._llm_config)
            await self._llm.start()
            self._log.info("LLM engine started for {}", self.platform_name)

    async def _get_or_generate_extract_fn(self, page) -> str | None:
        """获取或生成 JS 提取函数。缓存命中直接返回，否则调用 LLM 生成。"""
        await self._ensure_llm()
        if self._llm is None:
            return None

        cache_key = (
            urlparse(page.url).hostname + urlparse(page.url).path
        )
        if cache_key in self._extract_fn_cache:
            self._log.debug("extract fn cache hit: {}", cache_key)
            return self._extract_fn_cache[cache_key]

        pw = page.raw()
        try:
            skeleton = await pw.evaluate(_SKELETON_JS)
        except Exception as e:
            self._log.warning("skeleton JS failed: {}", e)
            return None

        if not skeleton or not isinstance(skeleton, list) or len(skeleton) == 0:
            self._log.info("skeleton returned no candidates")
            return None

        self._log.info("skeleton extracted {} candidates for {}",
                       len(skeleton), cache_key)

        try:
            result = await self._llm.chat([{
                "role": "user",
                "content": _EXTRACT_PROMPT + json.dumps(
                    skeleton, ensure_ascii=False
                ),
            }])
        except Exception as e:
            self._log.warning("LLM chat failed: {}", e)
            return None

        code = result.get("content", "").strip()
        extract_fn = self._parse_js(code)
        if extract_fn is None:
            self._log.warning("LLM returned no valid extractPosts function")
            return None

        # Validate: try executing it
        try:
            await pw.evaluate(extract_fn + "; extractPosts()")
        except Exception as e:
            self._log.warning(
                "LLM-generated extractPosts failed to execute: {}", e
            )
            return None

        self._extract_fn_cache[cache_key] = extract_fn
        self._log.info("extract fn cached for {}", cache_key)
        return extract_fn

    @staticmethod
    def _parse_js(code: str) -> str | None:
        """Strip markdown fences and validate contains extractPosts."""
        code = code.strip()
        if code.startswith("```"):
            lines = code.split("\n")
            start = 1 if lines[0].startswith("```") else 0
            end = -1 if lines[-1].startswith("```") else len(lines)
            code = "\n".join(lines[start:end])
        if "function extractPosts" in code:
            return code
        return None

    async def _llm_extract(
        self, page, extract_fn: str, limit: int
    ) -> AsyncIterator[UnifiedItem]:
        """使用 LLM 生成的 extractPosts() 提取帖子。"""
        pw = page.raw()
        extracted: set[str] = set()
        prev_count = 0
        stall_streak = 0

        while stall_streak < 3 and len(extracted) < limit:
            try:
                items_data = await pw.evaluate(
                    extract_fn + "; extractPosts()"
                )
            except Exception:
                break

            if not isinstance(items_data, list):
                break

            for data in items_data:
                if len(extracted) >= limit:
                    break
                item = self.extract(data)
                uid = item.item_id
                if uid and uid not in extracted:
                    extracted.add(uid)
                    yield item

            if len(extracted) >= limit:
                break

            await pw.evaluate("window.scrollBy(0, window.innerHeight)")
            await asyncio.sleep(self._config.scroll_delay)

            try:
                new_items = await pw.evaluate(
                    extract_fn + "; extractPosts()"
                )
                new_count = len(new_items) if isinstance(new_items, list) else 0
            except Exception:
                new_count = 0

            if new_count <= prev_count:
                stall_streak += 1
            else:
                stall_streak = 0
            prev_count = new_count

    async def _stop_llm(self):
        if self._llm is not None:
            await self._llm.stop()
            self._llm = None
