import json

import pytest

from src.common.models import UnifiedItem
from src.wheel.storage.imp.jsonl_storage import JsonlStorage
from src.wheel.storage.storage_factory import StorageFactory
from src.wheel.storage.storage_models import StorageConfig


class TestJsonlStorageTA:
    @pytest.fixture
    def storage(self, tmp_path):
        data_dir = str(tmp_path / "data")
        return JsonlStorage(data_dir)

    @pytest.fixture
    async def started_storage(self, storage):
        await storage.start()
        return storage

    @pytest.fixture
    def sample_items(self):
        return [
            UnifiedItem(
                platform="twitter",
                item_id="1",
                author_name="Alice",
                content_text="Hello",
            ),
            UnifiedItem(
                platform="rednote",
                item_id="2",
                author_name="Bob",
                content_text="你好",
            ),
            UnifiedItem(
                platform="twitter",
                item_id="3",
                author_name="Charlie",
                content_text="Bonjour",
            ),
        ]

    def test_name(self, storage):
        assert storage.name == "JsonlStorage"

    @pytest.mark.asyncio
    async def test_start_creates_dir(self, storage):
        await storage.start()
        assert storage.health_check()

    @pytest.mark.asyncio
    async def test_start_idempotent(self, storage):
        await storage.start()
        await storage.start()
        assert storage.health_check()

    @pytest.mark.asyncio
    async def test_stop_does_not_raise(self, storage):
        await storage.start()
        await storage.stop()

    def test_health_check_false_before_start(self, storage):
        assert storage.health_check() is False

    @pytest.mark.asyncio
    async def test_save_and_load_items(self, started_storage, sample_items):
        await started_storage.save_items(sample_items, date="2026-01-01")
        loaded = await started_storage.load_items(date="2026-01-01")
        assert len(loaded) == 3

    @pytest.mark.asyncio
    async def test_load_items_empty_dir(self, started_storage):
        items = await started_storage.load_items(date="2099-01-01")
        assert items == []

    @pytest.mark.asyncio
    async def test_load_items_filter_platform(self, started_storage, sample_items):
        await started_storage.save_items(sample_items, date="2026-01-01")
        tw = await started_storage.load_items(date="2026-01-01", platform="twitter")
        assert len(tw) == 2
        assert all(i.platform == "twitter" for i in tw)

    @pytest.mark.asyncio
    async def test_save_items_append(self, started_storage, sample_items):
        await started_storage.save_items(sample_items[:1], date="2026-01-01")
        await started_storage.save_items(sample_items[1:], date="2026-01-01")
        loaded = await started_storage.load_items(date="2026-01-01")
        assert len(loaded) == 3

    @pytest.mark.asyncio
    async def test_load_items_filters_nonexistent_platform(self, started_storage, sample_items):
        await started_storage.save_items(sample_items, date="2026-01-01")
        fb = await started_storage.load_items(date="2026-01-01", platform="facebook")
        assert fb == []

    @pytest.mark.asyncio
    async def test_save_and_load_report(self, started_storage):
        await started_storage.save_report("# Hello World", date="2026-01-01")
        report = await started_storage.load_report(date="2026-01-01")
        assert report == "# Hello World"

    @pytest.mark.asyncio
    async def test_load_report_missing(self, started_storage):
        report = await started_storage.load_report(date="2099-01-01")
        assert report is None

    @pytest.mark.asyncio
    async def test_save_report_overwrite(self, started_storage):
        await started_storage.save_report("# v1", date="2026-01-01")
        await started_storage.save_report("# v2", date="2026-01-01")
        report = await started_storage.load_report(date="2026-01-01")
        assert report == "# v2"

    @pytest.mark.asyncio
    async def test_save_and_load_summary(self, started_storage):
        summary = {"count": 5, "platforms": ["twitter"]}
        await started_storage.save_summary(summary, date="2026-01-01")
        loaded = await started_storage.load_summary(date="2026-01-01")
        assert loaded == summary

    @pytest.mark.asyncio
    async def test_load_summary_missing(self, started_storage):
        summary = await started_storage.load_summary(date="2099-01-01")
        assert summary is None

    @pytest.mark.asyncio
    async def test_list_dates_empty(self, started_storage):
        dates = await started_storage.list_dates()
        assert dates == []

    @pytest.mark.asyncio
    async def test_list_dates_reverse_order(self, started_storage, sample_items):
        await started_storage.save_items(sample_items, date="2026-01-01")
        await started_storage.save_items(sample_items, date="2026-01-03")
        await started_storage.save_items(sample_items, date="2026-01-02")
        dates = await started_storage.list_dates()
        assert dates == ["2026-01-03", "2026-01-02", "2026-01-01"]

    @pytest.mark.asyncio
    async def test_list_dates_filters_sessions(self, started_storage, tmp_path):
        (tmp_path / "data" / "sessions").mkdir(parents=True, exist_ok=True)
        await started_storage.save_items([], date="2026-01-01")
        assert "sessions" not in await started_storage.list_dates()

    @pytest.mark.asyncio
    async def test_full_roundtrip_with_llm_fields(self, started_storage):
        item = UnifiedItem(
            platform="twitter",
            item_id="rt1",
            author_name="Test",
            content_text="Content",
            llm_relevance_score=8,
            llm_relevance_reason="相关话题",
            llm_summary="测试摘要",
            llm_tags=["tag1", "tag2"],
            llm_key_points=["信息点1", "信息点2"],
            metrics={"likes": 10, "comments": 3},
        )
        await started_storage.save_items([item], date="2026-01-15")
        loaded = await started_storage.load_items(date="2026-01-15")
        assert len(loaded) == 1
        l = loaded[0]
        assert l.platform == "twitter"
        assert l.item_id == "rt1"
        assert l.llm_relevance_score == 8
        assert l.llm_tags == ["tag1", "tag2"]
        assert l.llm_key_points == ["信息点1", "信息点2"]
        assert l.metrics == {"likes": 10, "comments": 3}

    @pytest.mark.asyncio
    async def test_multi_date_isolation(self, started_storage, sample_items):
        await started_storage.save_items(sample_items[:1], date="2026-04-01")
        await started_storage.save_items(sample_items[1:], date="2026-04-02")
        d1 = await started_storage.load_items(date="2026-04-01")
        d2 = await started_storage.load_items(date="2026-04-02")
        assert len(d1) == 1
        assert len(d2) == 2
        assert d1[0].item_id == "1"

    @pytest.mark.asyncio
    async def test_save_items_with_scheduler(self, started_storage, sample_items):
        await started_storage.save_items(sample_items, date="2026-06-01")
        loaded = await started_storage.load_items(date="2026-06-01")
        assert len(loaded) == 3


class TestStorageFactoryWithSchedulerUT:
    def test_creates_with_internal_scheduler(self):
        storage = StorageFactory().create(StorageConfig(data_dir="/tmp/test"))
        assert isinstance(storage, JsonlStorage)
        assert storage._scheduler is not None

    def test_creates_without_scheduler(self):
        storage = StorageFactory().create(None)
        assert isinstance(storage, JsonlStorage)
        assert storage._scheduler is not None
