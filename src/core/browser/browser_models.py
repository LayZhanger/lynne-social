from dataclasses import dataclass


@dataclass
class BrowserConfig:
    headless: bool = False
    slow_mo: int = 500
    viewport_width: int = 1920
    viewport_height: int = 1080
    locale: str = "zh-CN"
    sessions_dir: str = "data/sessions"
