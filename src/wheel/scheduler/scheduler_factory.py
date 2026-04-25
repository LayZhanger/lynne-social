from src.common.factory import Factory
from src.wheel.scheduler.scheduler import Scheduler
from src.wheel.scheduler.scheduler_models import SchedulerConfig
from src.wheel.scheduler.imp.simple_scheduler import SimpleScheduler


class SchedulerFactory(Factory[Scheduler]):
    def create(self, config: object) -> Scheduler:
        if isinstance(config, SchedulerConfig):
            return SimpleScheduler(config)
        return SimpleScheduler(SchedulerConfig())
