import json
import urllib.error
import urllib.request

from src.wheel.llm.llm_engine import LLMEngine
from src.wheel.llm.llm_models import LLMConfig
from src.wheel.logger import get_logger
from src.wheel.scheduler.scheduler_factory import SchedulerFactory


class DeepSeekEngine(LLMEngine):
    _BASE_URL = "https://api.deepseek.com/v1"

    def __init__(self, config: LLMConfig):
        self._config = config
        self._scheduler = SchedulerFactory().create(None)
        self._log = get_logger("llm")
        self._base_url = config.base_url or self._BASE_URL

    async def start(self) -> None:
        await self._scheduler.start()
        self._log.info("LLM started: model={}", self._config.model)

    async def stop(self) -> None:
        await self._scheduler.stop()

    async def chat(
        self, messages: list[dict], tools: list[dict] | None = None
    ) -> dict:
        return await self._scheduler.run_blocking(
            self._chat_sync, messages, tools
        )

    def _chat_sync(
        self, messages: list[dict], tools: list[dict] | None
    ) -> dict:
        body: dict = {
            "model": self._config.model,
            "messages": messages,
            "temperature": self._config.temperature,
            "max_tokens": self._config.max_tokens,
        }
        if tools:
            body["tools"] = tools

        body_json = json.dumps(body, ensure_ascii=False)
        self._log.info("chat request → model={} msgs={} tools={} body={}",
                        self._config.model, len(messages),
                        len(tools) if tools else 0, body_json[:500])

        req = urllib.request.Request(
            f"{self._base_url}/chat/completions",
            data=body_json.encode("utf-8"),
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self._config.api_key}",
            },
        )

        try:
            with urllib.request.urlopen(req, timeout=self._config.timeout) as resp:
                raw = resp.read().decode("utf-8")
                data = json.loads(raw)
                self._log.info("chat response ← status={} body={}",
                               resp.status, raw[:500])
        except urllib.error.HTTPError as e:
            body_text = ""
            try:
                body_text = e.read().decode("utf-8", errors="replace")
            except Exception:
                pass
            self._log.error("chat error ← status={} body={}", e.code, body_text[:300])
            raise RuntimeError(
                f"LLM API returned {e.code}: {body_text[:200]}"
            ) from e
        except urllib.error.URLError as e:
            self._log.error("chat error ← reason={}", e.reason)
            raise RuntimeError(f"LLM API unreachable: {e.reason}") from e

        choice = data["choices"][0]
        msg = choice["message"]
        result: dict = {"role": "assistant", "content": msg.get("content") or ""}
        if msg.get("tool_calls"):
            result["tool_calls"] = msg["tool_calls"]
        return result

    def health_check(self) -> bool:
        try:
            req = urllib.request.Request(
                f"{self._base_url}/models",
                headers={
                    "Authorization": f"Bearer {self._config.api_key}"
                },
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                return 200 <= resp.status < 500
        except Exception:
            return False

    @property
    def name(self) -> str:
        return f"llm({self._config.provider}:{self._config.model})"
