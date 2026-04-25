from src.core.adapters.adapter_models import AdapterConfig


class TestAdapterConfigUT:
    def test_defaults(self):
        cfg = AdapterConfig()
        assert cfg.platform == "rednote"
        assert cfg.base_url == "https://www.xiaohongshu.com"
        assert "{keyword}" in cfg.search_url
        assert "{user_id}" in cfg.user_url_template
        assert cfg.trending_url == "https://www.xiaohongshu.com/explore"
        assert cfg.max_scrolls == 20
        assert cfg.scroll_delay == 1.5
        assert cfg.page_timeout == 30000

    def test_custom_values(self):
        cfg = AdapterConfig(
            platform="twitter",
            base_url="https://x.com",
            search_url="https://x.com/search?q={keyword}",
            user_url_template="https://x.com/{user_id}",
            trending_url="https://x.com/trending",
            max_scrolls=10,
            scroll_delay=0.5,
            page_timeout=15000,
        )
        assert cfg.platform == "twitter"
        assert cfg.base_url == "https://x.com"
        assert cfg.max_scrolls == 10
        assert cfg.scroll_delay == 0.5

    def test_equality(self):
        a = AdapterConfig()
        b = AdapterConfig()
        assert a == b

    def test_inequality(self):
        a = AdapterConfig(platform="twitter")
        b = AdapterConfig(platform="rednote")
        assert a != b

    def test_search_url_keyword_interpolation(self):
        cfg = AdapterConfig(search_url="https://search?q={keyword}")
        replaced = cfg.search_url.replace("{keyword}", "AI")
        assert replaced == "https://search?q=AI"

    def test_user_url_interpolation(self):
        cfg = AdapterConfig(user_url_template="/user/{user_id}")
        assert cfg.user_url_template.replace("{user_id}", "profile123") == "/user/profile123"
