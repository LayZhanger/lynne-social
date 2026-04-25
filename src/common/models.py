from dataclasses import dataclass, field, asdict
import json


@dataclass
class UnifiedItem:
    platform: str
    item_id: str
    item_type: str = "post"
    author_id: str = ""
    author_name: str = ""
    content_text: str = ""
    content_media: list[str] = field(default_factory=list)
    url: str = ""
    published_at: str = ""
    fetched_at: str = ""
    metrics: dict = field(default_factory=dict)

    llm_relevance_score: int = 0
    llm_relevance_reason: str = ""
    llm_summary: str = ""
    llm_tags: list[str] = field(default_factory=list)
    llm_key_points: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return asdict(self)

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), ensure_ascii=False)

    @classmethod
    def from_dict(cls, d: dict) -> "UnifiedItem":
        return cls(
            platform=d.get("platform", ""),
            item_id=d.get("item_id", ""),
            item_type=d.get("item_type", "post"),
            author_id=d.get("author_id", ""),
            author_name=d.get("author_name", ""),
            content_text=d.get("content_text", ""),
            content_media=d.get("content_media", []) or [],
            url=d.get("url", ""),
            published_at=d.get("published_at", ""),
            fetched_at=d.get("fetched_at", ""),
            metrics=d.get("metrics", {}) or {},
            llm_relevance_score=d.get("llm_relevance_score", 0),
            llm_relevance_reason=d.get("llm_relevance_reason", ""),
            llm_summary=d.get("llm_summary", ""),
            llm_tags=d.get("llm_tags", []) or [],
            llm_key_points=d.get("llm_key_points", []) or [],
        )


@dataclass
class TaskResult:
    task_name: str
    fetched_count: int = 0
    kept_count: int = 0
    llm_calls: int = 0
    report_markdown: str = ""
    duration_seconds: float = 0.0
    items: list[UnifiedItem] = field(default_factory=list)


@dataclass
class RunStatus:
    running: bool = False
    current_task: str | None = None
    progress: str = ""
