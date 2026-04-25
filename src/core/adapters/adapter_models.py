from dataclasses import dataclass


@dataclass
class AdapterConfig:
    platform: str = "rednote"
    base_url: str = "https://www.xiaohongshu.com"
    search_url: str = "https://www.xiaohongshu.com/search_result?keyword={keyword}"
    user_url_template: str = "https://www.xiaohongshu.com/user/profile/{user_id}"
    trending_url: str = "https://www.xiaohongshu.com/explore"
    max_scrolls: int = 20
    scroll_delay: float = 1.5
    page_timeout: int = 30000
