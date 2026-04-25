import sys

from loguru import logger as _logger

from .logger_models import LogConfig

_configured = False


def _configure(config: LogConfig | None = None):
    global _configured
    if _configured:
        return
    _configured = True

    cfg = config or LogConfig()

    _logger.remove()

    fmt = (
        "<green>{time:HH:mm:ss}</green> | "
        "<level>{level: <5}</level> | "
        "<cyan>{extra[module]: <18}</cyan> | "
        "<level>{message}</level>"
    )

    _logger.add(sys.stderr, format=fmt, level=cfg.level, colorize=True)

    _logger.add(
        cfg.log_file,
        format=fmt,
        level="DEBUG",
        rotation=cfg.rotation,
        retention=cfg.retention,
        encoding="utf-8",
    )


def get_logger(module: str):
    _configure()
    return _logger.bind(module=module)
