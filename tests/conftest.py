from pathlib import Path

import pytest

from src.common.models import UnifiedItem


@pytest.fixture
def sample_item() -> UnifiedItem:
    return UnifiedItem(
        platform="twitter",
        item_id="123",
        author_name="test_user",
        content_text="Hello world",
    )


@pytest.fixture
def sample_items(sample_item) -> list[UnifiedItem]:
    return [
        sample_item,
        UnifiedItem(platform="rednote", item_id="456", content_text="小红书"),
        UnifiedItem(platform="twitter", item_id="789", content_text="Another"),
    ]


@pytest.fixture
def sample_yaml_content() -> str:
    return """server:
  port: 8888
  auto_open_browser: false
llm:
  provider: openai
  api_key: sk-test
  model: gpt-4
  relevance_threshold: 7
browser:
  headless: true
  slow_mo: 200
platforms:
  twitter:
    enabled: true
    session_file: data/sessions/twitter.json
    base_url: https://x.com
tasks:
  - name: "AI动态"
    platforms: ["twitter"]
    topic: "AI news"
    keywords: ["AI", "LLM"]
    schedule: "every 4 hours"
"""


@pytest.fixture
def sample_yaml_file(tmp_path, sample_yaml_content) -> Path:
    path = tmp_path / "config.yaml"
    path.write_text(sample_yaml_content, encoding="utf-8")
    return path
