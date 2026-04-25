from dataclasses import dataclass


@dataclass
class AdapterConfig:
    platform: str = ""
    max_scrolls: int = 20
    scroll_delay: float = 1.5
    page_timeout: int = 30000
