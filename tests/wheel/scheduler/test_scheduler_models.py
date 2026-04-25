from src.wheel.scheduler.scheduler_models import SchedulerConfig


class TestSchedulerConfigUT:
    def test_defaults(self):
        cfg = SchedulerConfig()
        assert cfg.timezone == "Asia/Shanghai"
        assert cfg.max_workers == 4

    def test_custom_values(self):
        cfg = SchedulerConfig(timezone="UTC", max_workers=8)
        assert cfg.timezone == "UTC"
        assert cfg.max_workers == 8

    def test_equality(self):
        a = SchedulerConfig()
        b = SchedulerConfig()
        assert a == b

    def test_inequality(self):
        a = SchedulerConfig(max_workers=4)
        b = SchedulerConfig(max_workers=8)
        assert a != b
