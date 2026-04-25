import json
import os
from io import BytesIO
from unittest.mock import patch

import pytest

from src.wheel.llm.imp.deepseek_engine import DeepSeekEngine
from src.wheel.llm.llm_models import LLMConfig


def _fake_urlopen_ok(body_bytes: bytes):
    data = json.loads(body_bytes)
    content = json.dumps({
        "choices": [{
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "Hello, how can I help?",
            },
            "finish_reason": "stop",
        }],
    }).encode("utf-8")
    return BytesIO(content)


def _fake_urlopen_tool_calls(body_bytes: bytes):
    content = json.dumps({
        "choices": [{
            "index": 0,
            "message": {
                "role": "assistant",
                "content": None,
                "tool_calls": [{
                    "id": "call_001",
                    "type": "function",
                    "function": {
                        "name": "search",
                        "arguments": '{"keyword":"AI"}',
                    },
                }],
            },
            "finish_reason": "tool_calls",
        }],
    }).encode("utf-8")
    return BytesIO(content)


class TestDeepSeekEngineUT:
    @pytest.fixture(autouse=True)
    def _setup(self, tmp_path):
        self._tmp_path = tmp_path

    async def _make_engine(self, **kw) -> DeepSeekEngine:
        cfg_kw = {"api_key": "sk-test"}
        cfg_kw.update(kw)
        engine = DeepSeekEngine(LLMConfig(**cfg_kw))
        await engine.start()
        return engine

    # ── lifecycle ──

    @pytest.mark.asyncio
    async def test_start_stop(self):
        engine = await self._make_engine()
        assert engine.name == "llm(deepseek:deepseek-chat)"
        await engine.stop()

    def test_health_check_no_network(self):
        engine = DeepSeekEngine(LLMConfig(api_key="sk-test"))
        ok = engine.health_check()
        assert ok is False

    # ── chat ──

    @pytest.mark.asyncio
    async def test_chat_plain(self):
        engine = await self._make_engine()

        def _openurl(req, **kw):
            body = req.data or b"{}"
            return _fake_urlopen_ok(body)

        with patch("urllib.request.urlopen", _openurl):
            result = await engine.chat([{"role": "user", "content": "hi"}])

        assert result["role"] == "assistant"
        assert result["content"] == "Hello, how can I help?"
        assert "tool_calls" not in result

        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_with_tool_calls(self):
        engine = await self._make_engine()

        def _openurl(req, **kw):
            return _fake_urlopen_tool_calls(req.data or b"{}")

        with patch("urllib.request.urlopen", _openurl):
            result = await engine.chat(
                [{"role": "user", "content": "search AI"}],
                tools=[{"type": "function", "function": {"name": "search", "parameters": {}}}],
            )

        assert result["role"] == "assistant"
        assert result["content"] == ""
        assert result["tool_calls"][0]["function"]["name"] == "search"
        assert result["tool_calls"][0]["function"]["arguments"] == '{"keyword":"AI"}'

        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_includes_tools_in_request(self):
        engine = await self._make_engine()
        sent_body: dict = {}

        def _openurl(req, **kw):
            nonlocal sent_body
            sent_body = json.loads(req.data)
            return _fake_urlopen_ok(req.data)

        tools = [{"type": "function", "function": {"name": "search", "parameters": {}}}]
        with patch("urllib.request.urlopen", _openurl):
            await engine.chat([{"role": "user", "content": "x"}], tools=tools)

        assert "tools" in sent_body
        assert sent_body["tools"] == tools

        await engine.stop()

    # ── error handling ──

    @pytest.mark.asyncio
    async def test_chat_http_error(self):
        from urllib.error import HTTPError

        engine = await self._make_engine()

        def _openurl(req, **kw):
            raise HTTPError(
                url="https://api.deepseek.com/v1/chat/completions",
                code=401,
                msg="Unauthorized",
                hdrs={},
                fp=BytesIO(b'{"error":{"message":"Invalid API Key"}}'),
            )

        with patch("urllib.request.urlopen", _openurl):
            with pytest.raises(RuntimeError, match="401"):
                await engine.chat([{"role": "user", "content": "hi"}])

        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_http_error_with_unreadable_body(self):
        from urllib.error import HTTPError

        engine = await self._make_engine()

        class _UnreadableFP:
            def read(self):
                raise OSError("read failed")

        def _openurl(req, **kw):
            raise HTTPError(
                url="https://api.deepseek.com/v1/chat/completions",
                code=500,
                msg="Error",
                hdrs={},
                fp=_UnreadableFP(),
            )

        with patch("urllib.request.urlopen", _openurl):
            with pytest.raises(RuntimeError, match="500"):
                await engine.chat([{"role": "user", "content": "hi"}])

        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_urllib_error(self):
        from urllib.error import URLError

        engine = await self._make_engine()

        def _openurl(req, **kw):
            raise URLError("connection refused")

        with patch("urllib.request.urlopen", _openurl):
            with pytest.raises(RuntimeError, match="connection refused"):
                await engine.chat([{"role": "user", "content": "hi"}])

        await engine.stop()

    # ── custom config ──

    @pytest.mark.asyncio
    async def test_custom_base_url(self):
        engine = await self._make_engine(base_url="https://custom.llm/api/v1")
        sent_url: str = ""

        def _openurl(req, **kw):
            nonlocal sent_url
            sent_url = req.full_url
            return _fake_urlopen_ok(req.data)

        with patch("urllib.request.urlopen", _openurl):
            await engine.chat([{"role": "user", "content": "hi"}])

        assert sent_url.startswith("https://custom.llm/api/v1/chat/completions")

        await engine.stop()

    @pytest.mark.asyncio
    async def test_custom_temperature_and_max_tokens(self):
        engine = await self._make_engine(temperature=0.3, max_tokens=512)
        sent_body: dict = {}

        def _openurl(req, **kw):
            nonlocal sent_body
            sent_body = json.loads(req.data)
            return _fake_urlopen_ok(req.data)

        with patch("urllib.request.urlopen", _openurl):
            await engine.chat([{"role": "user", "content": "hi"}])

        assert sent_body["temperature"] == 0.3
        assert sent_body["max_tokens"] == 512

        await engine.stop()

    @pytest.mark.asyncio
    async def test_name_reflects_provider_and_model(self):
        engine = await self._make_engine(model="deepseek-reasoner")
        assert engine.name == "llm(deepseek:deepseek-reasoner)"
        await engine.stop()


# ═════════════════════════════════════════════════════════════════
# TA: DeepSeekEngine with real api.deepseek.com
# Key chain: env DEEPSEEK_API_KEY → config.yaml ${DEEPSEEK_API_KEY}
#   → YamlConfigLoader.load() → Config.llm → LLMConfig dataclass
#   → DeepSeekEngine
# ═════════════════════════════════════════════════════════════════

from pathlib import Path


class TestDeepSeekEngineTA:
    @pytest.fixture(autouse=True)
    async def _setup(self):
        from src.wheel.config.imp.yaml_config_loader import YamlConfigLoader

        project_root = Path(__file__).resolve().parents[3]
        config_path = project_root / "config.yaml"
        self._loader = YamlConfigLoader(str(config_path))
        self._yaml_config = await self._loader.load()

        key = self._yaml_config.llm.api_key
        if not key or "placeholder" in key.lower():
            key = os.environ.get("DEEPSEEK_API_KEY", "")
        if not key:
            pytest.skip("LLM api_key not set in config.yaml or DEEPSEEK_API_KEY env")

    def _cfg_from_yaml(self) -> LLMConfig:
        llm = self._yaml_config.llm
        api_key = llm.api_key
        if not api_key or "placeholder" in api_key.lower():
            api_key = os.environ.get("DEEPSEEK_API_KEY", "")
        return LLMConfig(
            provider=llm.provider,
            api_key=api_key,
            base_url=llm.base_url,
            model=llm.model,
            temperature=llm.temperature,
            max_tokens=llm.max_tokens,
            timeout=llm.timeout,
        )

    async def _make_engine(self, **kw) -> DeepSeekEngine:
        cfg = self._cfg_from_yaml()
        for k, v in kw.items():
            setattr(cfg, k, v)
        engine = DeepSeekEngine(cfg)
        await engine.start()
        return engine

    # ── health ──

    async def test_health_check_real(self):
        cfg = self._cfg_from_yaml()
        engine = DeepSeekEngine(cfg)
        ok = engine.health_check()
        assert ok is True

    @pytest.mark.asyncio
    async def test_lifecycle_real(self):
        engine = await self._make_engine()
        assert "llm(deepseek" in engine.name
        await engine.stop()

    # ── chat ──

    @pytest.mark.asyncio
    async def test_chat_real_plain(self):
        engine = await self._make_engine()
        result = await engine.chat([
            {"role": "user", "content": "Say exactly: hello world"},
        ])
        assert result["role"] == "assistant"
        assert "hello world" in result["content"].lower()
        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_real_chinese(self):
        engine = await self._make_engine()
        result = await engine.chat([
            {"role": "user", "content": "用中文回答：1+1等于几？只回答数字。"},
        ])
        assert result["role"] == "assistant"
        assert result["content"], "expected non-empty response"
        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_real_multi_turn(self):
        engine = await self._make_engine()
        result = await engine.chat([
            {"role": "user", "content": "My name is Alice."},
            {"role": "assistant", "content": "Nice to meet you, Alice."},
            {"role": "user", "content": "What is my name?"},
        ])
        assert "Alice" in result["content"]
        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_real_with_tools(self):
        engine = await self._make_engine()
        tools = [{
            "type": "function",
            "function": {
                "name": "get_weather",
                "description": "Get current weather for a city",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "city": {"type": "string"},
                    },
                    "required": ["city"],
                },
            },
        }]
        result = await engine.chat([
            {"role": "user", "content": "What is the weather in Beijing?"},
        ], tools=tools)

        assert result["role"] == "assistant"
        if "tool_calls" in result:
            tc = result["tool_calls"][0]
            assert tc["type"] == "function"
            assert tc["function"]["name"] == "get_weather"
            args = json.loads(tc["function"]["arguments"])
            assert "city" in args
        else:
            assert result["content"], "expected content or tool_calls"
        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_real_keyword_search_tool(self):
        engine = await self._make_engine()
        tools = [{
            "type": "function",
            "function": {
                "name": "search",
                "description": "Search for posts on a social platform",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "platform": {"type": "string", "enum": ["rednote", "twitter"]},
                        "keywords": {"type": "array", "items": {"type": "string"}},
                        "limit": {"type": "integer"},
                    },
                    "required": ["platform", "keywords"],
                },
            },
        }]
        result = await engine.chat([
            {"role": "user", "content": "搜一下小红书上的AI模型最新消息"},
        ], tools=tools)

        assert result["role"] == "assistant"
        if "tool_calls" in result:
            tc = result["tool_calls"][0]
            assert tc["function"]["name"] == "search"
            args = json.loads(tc["function"]["arguments"])
            assert args["platform"] == "rednote"
        else:
            assert result["content"], "expected content or tool_calls"
        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_real_system_prompt(self):
        engine = await self._make_engine()
        result = await engine.chat([
            {"role": "system", "content": "You are a JSON machine. Output only JSON, no markdown, no explanation."},
            {"role": "user", "content": 'Return: {"ok": true}'},
        ])
        assert result["role"] == "assistant"
        content = result["content"].strip()
        assert content, "expected non-empty response"
        await engine.stop()

    # ── config ──

    @pytest.mark.asyncio
    async def test_chat_real_custom_model(self):
        engine = await self._make_engine(model="deepseek-reasoner")
        result = await engine.chat([
            {"role": "user", "content": "Say: ok"},
        ])
        assert result["role"] == "assistant"
        assert result["content"]
        await engine.stop()

    @pytest.mark.asyncio
    async def test_chat_real_low_temperature(self):
        engine = await self._make_engine(temperature=0.0)
        result1 = await engine.chat([
            {"role": "user", "content": "Reply with exactly: deterministic"},
        ])
        result2 = await engine.chat([
            {"role": "user", "content": "Reply with exactly: deterministic"},
        ])
        assert result1["role"] == "assistant"
        assert result2["role"] == "assistant"
        await engine.stop()
