import os
import re
from pathlib import Path

import yaml

from src.wheel.scheduler.scheduler_factory import SchedulerFactory

from ..config_loader import ConfigLoader
from ..config_models import Config

_ENV_VAR_RE = re.compile(r"\$\{(\w+)\}")


def _resolve_env(value: str) -> str:
    def _replace(m):
        var_name = m.group(1)
        env_val = os.environ.get(var_name, "")
        if not env_val:
            raise ValueError(f"环境变量 {var_name} 未设置")
        return env_val

    return _ENV_VAR_RE.sub(_replace, value)


def _resolve_env_recursive(obj):
    if isinstance(obj, str):
        return _resolve_env(obj)
    elif isinstance(obj, dict):
        return {k: _resolve_env_recursive(v) for k, v in obj.items()}
    elif isinstance(obj, list):
        return [_resolve_env_recursive(i) for i in obj]
    return obj


class YamlConfigLoader(ConfigLoader):
    def __init__(self, path: str = "config.yaml"):
        self._path = Path(path)
        self._config: Config | None = None
        self._scheduler = SchedulerFactory().create(None)

    async def load(self) -> Config:
        self._config = await self._load_from_file()
        return self._config

    async def reload(self) -> Config:
        return await self.load()

    async def _load_from_file(self) -> Config:
        raw_text = await self._scheduler.run_blocking(self._read_file_sync)

        if raw_text is None:
            return Config()

        raw = yaml.safe_load(raw_text)
        if raw is None:
            return Config()

        raw = _resolve_env_recursive(raw)
        return Config.model_validate(raw)

    def _read_file_sync(self) -> str | None:
        if not self._path.exists():
            return None
        return self._path.read_text(encoding="utf-8")

    @property
    def config(self) -> Config:
        if self._config is None:
            raise RuntimeError("配置未加载")
        return self._config
