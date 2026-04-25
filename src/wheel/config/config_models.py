from pydantic import BaseModel, Field


class ServerConfig(BaseModel):
    port: int = 7890
    auto_open_browser: bool = True


class LLMConfig(BaseModel):
    provider: str = "deepseek"
    api_key: str = ""
    base_url: str = ""
    model: str = "deepseek-chat"
    model_filter: str = ""
    model_extract: str = ""
    model_report: str = ""
    relevance_threshold: int = 6
    temperature: float = 0.3
    max_tokens: int = 200
    timeout: int = 60


class BrowserConfig(BaseModel):
    headless: bool = False
    slow_mo: int = 500
    viewport_width: int = 1920
    viewport_height: int = 1080
    locale: str = "zh-CN"
    timeout: int = 30000


class PlatformConfig(BaseModel):
    enabled: bool = False
    session_file: str = ""
    base_url: str = ""


class TaskConfig(BaseModel):
    name: str
    platforms: list[str] = Field(default_factory=list)
    topic: str = ""
    keywords: list[str] = Field(default_factory=list)
    user_ids: list[str] = Field(default_factory=list)
    schedule: str = "manual"
    limit: int = 20


class Config(BaseModel):
    server: ServerConfig = Field(default_factory=ServerConfig)
    llm: LLMConfig = Field(default_factory=LLMConfig)
    browser: BrowserConfig = Field(default_factory=BrowserConfig)
    platforms: dict[str, PlatformConfig] = Field(default_factory=dict)
    tasks: list[TaskConfig] = Field(default_factory=list)
