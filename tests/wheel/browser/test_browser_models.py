from src.wheel.browser.browser_models import BrowserConfig


class TestBrowserConfigUT:
    def test_defaults(self):
        cfg = BrowserConfig()
        assert cfg.headless is False
        assert cfg.slow_mo == 500
        assert cfg.viewport_width == 1920
        assert cfg.viewport_height == 1080
        assert cfg.locale == "zh-CN"
        assert cfg.sessions_dir == "data/sessions"

    def test_custom_values(self):
        cfg = BrowserConfig(
            headless=True,
            slow_mo=0,
            viewport_width=1280,
            viewport_height=720,
            locale="en-US",
            sessions_dir="/tmp/sessions",
        )
        assert cfg.headless is True
        assert cfg.slow_mo == 0
        assert cfg.viewport_width == 1280
        assert cfg.viewport_height == 720
        assert cfg.locale == "en-US"
        assert cfg.sessions_dir == "/tmp/sessions"

    def test_equality(self):
        a = BrowserConfig()
        b = BrowserConfig()
        assert a == b

    def test_inequality(self):
        a = BrowserConfig(headless=True)
        b = BrowserConfig(headless=False)
        assert a != b
