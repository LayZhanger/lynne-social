import os
import re
from pathlib import Path

import yaml

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
        self.load()

    def load(self) -> Config:
        self._config = self._load_from_file()
        return self._config

    def reload(self) -> Config:
        return self.load()

    def _load_from_file(self) -> Config:
        if not self._path.exists():
            return Config()

        raw = yaml.safe_load(self._path.read_text(encoding="utf-8"))
        if raw is None:
            return Config()

        raw = _resolve_env_recursive(raw)
        return Config.model_validate(raw)

    @property
    def config(self) -> Config:
        if self._config is None:
            raise RuntimeError("配置未加载")
        return self._config
