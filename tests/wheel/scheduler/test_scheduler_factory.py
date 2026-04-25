from src.wheel.scheduler.scheduler import Scheduler
from src.wheel.scheduler.scheduler_factory import SchedulerFactory
from src.wheel.scheduler.scheduler_models import SchedulerConfig
from src.wheel.scheduler.imp.simple_scheduler import SimpleScheduler


class TestSchedulerFactoryUT:
    def test_create_default(self):
        sched = SchedulerFactory().create(None)
        assert isinstance(sched, SimpleScheduler)
        assert isinstance(sched, Scheduler)

    def test_create_with_config(self):
        cfg = SchedulerConfig(max_workers=2)
        sched = SchedulerFactory().create(cfg)
        assert isinstance(sched, SimpleScheduler)
        assert sched._config is cfg

    def test_create_with_non_config_returns_default(self):
        sched = SchedulerFactory().create("not a config")
        assert isinstance(sched, SimpleScheduler)
        assert sched._config.max_workers == 4
