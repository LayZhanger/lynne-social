from dataclasses import dataclass, field


@dataclass
class AgentConfig:
    model: str = "deepseek-chat"
    max_steps: int = 10
    temperature: float = 0.7
    max_tokens: int = 4096


@dataclass
class AgentStep:
    step: int
    thought: str = ""
    action: str = ""
    observation: str = ""


@dataclass
class TaskConfig:
    name: str = ""
    platforms: list[str] = field(default_factory=lambda: ["rednote"])
    intent: str = ""
    schedule: str = "manual"
