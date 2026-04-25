from src.common import Factory

from .agent import Agent
from .agent_models import AgentConfig


class AgentFactory(Factory[Agent]):
    def create(self, config: object) -> Agent:
        raise NotImplementedError
