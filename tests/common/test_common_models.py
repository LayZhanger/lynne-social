import json

from src.common.models import RunStatus, TaskResult, UnifiedItem


class TestUnifiedItemUT:
    def test_create_with_defaults(self):
        item = UnifiedItem(platform="twitter", item_id="123")
        assert item.platform == "twitter"
        assert item.item_id == "123"
        assert item.item_type == "post"
        assert item.author_id == ""
        assert item.metrics == {}
        assert item.content_media == []
        assert item.llm_relevance_score == 0
        assert item.llm_tags == []

    def test_create_full(self):
        item = UnifiedItem(
            platform="rednote",
            item_id="456",
            item_type="video",
            author_id="u1",
            author_name="小红",
            content_text="好物分享",
            content_media=["https://img.jpg"],
            url="https://xhslink.com/abc",
            published_at="2026-01-01T08:00:00",
            fetched_at="2026-01-01T09:00:00",
            metrics={"likes": 100},
            llm_relevance_score=8,
            llm_relevance_reason="相关",
            llm_summary="推荐某产品",
            llm_tags=["好物"],
            llm_key_points=["性价比高"],
        )
        assert item.platform == "rednote"
        assert item.llm_relevance_score == 8
        assert item.llm_tags == ["好物"]

    def test_to_dict(self):
        item = UnifiedItem(platform="twitter", item_id="123", content_text="Hello")
        d = item.to_dict()
        assert d["platform"] == "twitter"
        assert d["item_id"] == "123"
        assert d["content_text"] == "Hello"
        assert d["content_media"] == []

    def test_to_json(self):
        item = UnifiedItem(platform="twitter", item_id="abc")
        js = item.to_json()
        data = json.loads(js)
        assert data["platform"] == "twitter"
        assert data["item_id"] == "abc"

    def test_from_dict_full(self):
        d = {
            "platform": "twitter",
            "item_id": "567",
            "item_type": "post",
            "author_name": "Elon",
            "content_text": "Starship",
            "llm_relevance_score": 9,
            "llm_tags": ["space"],
        }
        item = UnifiedItem.from_dict(d)
        assert item.platform == "twitter"
        assert item.item_id == "567"
        assert item.author_name == "Elon"
        assert item.llm_relevance_score == 9
        assert item.llm_tags == ["space"]

    def test_from_dict_empty(self):
        item = UnifiedItem.from_dict({})
        assert item.platform == ""
        assert item.item_id == ""
        assert item.item_type == "post"
        assert item.llm_tags == []
        assert item.content_media == []

    def test_from_dict_partial(self):
        item = UnifiedItem.from_dict({"platform": "douyin", "item_id": "999"})
        assert item.platform == "douyin"
        assert item.item_id == "999"
        assert item.content_text == ""
        assert item.llm_relevance_score == 0

    def test_from_dict_nulls(self):
        d = {
            "platform": "fb",
            "item_id": "1",
            "content_media": None,
            "llm_tags": None,
            "metrics": None,
        }
        item = UnifiedItem.from_dict(d)
        assert item.content_media == []
        assert item.llm_tags == []
        assert item.metrics == {}

    def test_roundtrip(self):
        item = UnifiedItem(
            platform="twitter",
            item_id="x",
            author_name="A",
            content_text="C",
            llm_relevance_score=7,
            llm_tags=["t"],
        )
        restored = UnifiedItem.from_dict(item.to_dict())
        assert restored.platform == item.platform
        assert restored.item_id == item.item_id
        assert restored.author_name == item.author_name
        assert restored.llm_relevance_score == item.llm_relevance_score
        assert restored.llm_tags == item.llm_tags


class TestTaskResultUT:
    def test_defaults(self):
        tr = TaskResult(task_name="test")
        assert tr.task_name == "test"
        assert tr.fetched_count == 0
        assert tr.kept_count == 0
        assert tr.llm_calls == 0
        assert tr.report_markdown == ""
        assert tr.duration_seconds == 0.0
        assert tr.items == []

    def test_with_items(self, sample_items):
        tr = TaskResult(
            task_name="run1",
            fetched_count=10,
            kept_count=5,
            llm_calls=3,
            report_markdown="# Report",
            duration_seconds=30.5,
            items=sample_items,
        )
        assert tr.fetched_count == 10
        assert tr.items == sample_items


class TestRunStatusUT:
    def test_defaults(self):
        rs = RunStatus()
        assert rs.running is False
        assert rs.current_task is None
        assert rs.progress == ""

    def test_running(self):
        rs = RunStatus(running=True, current_task="AI动态", progress="采集Twitter")
        assert rs.running is True
        assert rs.current_task == "AI动态"
        assert rs.progress == "采集Twitter"
