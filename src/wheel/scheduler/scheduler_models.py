from dataclasses import dataclass


@dataclass
class SchedulerConfig:
    """Configuration for the scheduler engine."""
    timezone: str = "Asia/Shanghai"
    max_workers: int = 4
