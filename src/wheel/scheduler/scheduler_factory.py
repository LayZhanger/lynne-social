from src.common.factory import Factory
from src.wheel.scheduler.scheduler import Scheduler


class SchedulerFactory(Factory[Scheduler]):
    """Factory for creating concrete Scheduler instances."""
    pass
