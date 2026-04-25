from dataclasses import dataclass


@dataclass
class LogConfig:
    level: str = "INFO"
    rotation: str = "10 MB"
    retention: str = "7 days"
    log_file: str = "data/lynne.log"
