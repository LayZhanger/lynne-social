from src.core.adapters.adapter_models import AdapterConfig


class TestAdapterConfigUT:
    def test_defaults(self):
        cfg = AdapterConfig()
        assert cfg.platform == ""
        assert cfg.max_scrolls == 20
        assert cfg.scroll_delay == 1.5
        assert cfg.page_timeout == 30000

    def test_custom_values(self):
        cfg = AdapterConfig(
            platform="twitter",
            max_scrolls=10,
            scroll_delay=0.5,
            page_timeout=15000,
        )
        assert cfg.platform == "twitter"
        assert cfg.max_scrolls == 10
        assert cfg.scroll_delay == 0.5
        assert cfg.page_timeout == 15000

    def test_equality(self):
        a = AdapterConfig()
        b = AdapterConfig()
        assert a == b

    def test_inequality(self):
        a = AdapterConfig(platform="twitter")
        b = AdapterConfig(platform="rednote")
        assert a != b
