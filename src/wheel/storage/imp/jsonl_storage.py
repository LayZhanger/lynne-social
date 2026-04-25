import json
from datetime import datetime
from pathlib import Path

from src.common import UnifiedItem
from src.wheel.scheduler.scheduler_factory import SchedulerFactory

from ..storage import Storage


class JsonlStorage(Storage):
    def __init__(self, data_dir: str = "data"):
        self._data_dir = Path(data_dir)
        self._scheduler = SchedulerFactory().create(None)

    @property
    def name(self) -> str:
        return "JsonlStorage"

    async def start(self) -> None:
        self._data_dir.mkdir(parents=True, exist_ok=True)

    async def stop(self) -> None:
        pass

    def health_check(self) -> bool:
        return self._data_dir.exists()

    def _date_dir(self, date: str) -> Path:
        d = self._data_dir / date
        d.mkdir(parents=True, exist_ok=True)
        return d

    @staticmethod
    def _today_str() -> str:
        return datetime.now().strftime("%Y-%m-%d")

    async def save_items(self, items: list[UnifiedItem], date: str | None = None) -> None:
        date = date or self._today_str()
        await self._scheduler.run_blocking(self._save_items_sync, items, date)

    def _save_items_sync(self, items: list[UnifiedItem], date: str) -> None:
        path = self._date_dir(date) / "items.jsonl"
        with open(path, "a", encoding="utf-8") as f:
            for item in items:
                f.write(item.to_json() + "\n")

    async def load_items(self, date: str | None = None, *, platform: str | None = None) -> list[UnifiedItem]:
        date = date or self._today_str()
        return await self._scheduler.run_blocking(self._load_items_sync, date, platform=platform)

    def _load_items_sync(self, date: str, *, platform: str | None = None) -> list[UnifiedItem]:
        path = self._data_dir / date / "items.jsonl"
        if not path.exists():
            return []
        items = []
        for line in path.read_text(encoding="utf-8").strip().splitlines():
            if not line:
                continue
            d = json.loads(line)
            item = UnifiedItem.from_dict(d)
            if platform and item.platform != platform:
                continue
            items.append(item)
        return items

    async def save_report(self, markdown: str, date: str | None = None) -> None:
        date = date or self._today_str()
        await self._scheduler.run_blocking(self._save_report_sync, markdown, date)

    def _save_report_sync(self, markdown: str, date: str) -> None:
        path = self._date_dir(date) / "report.md"
        path.write_text(markdown, encoding="utf-8")

    async def load_report(self, date: str | None = None) -> str | None:
        date = date or self._today_str()
        return await self._scheduler.run_blocking(self._load_report_sync, date)

    def _load_report_sync(self, date: str) -> str | None:
        path = self._data_dir / date / "report.md"
        if not path.exists():
            return None
        return path.read_text(encoding="utf-8")

    async def save_summary(self, summary: dict, date: str | None = None) -> None:
        date = date or self._today_str()
        await self._scheduler.run_blocking(self._save_summary_sync, summary, date)

    def _save_summary_sync(self, summary: dict, date: str) -> None:
        path = self._date_dir(date) / "summary.json"
        path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    async def load_summary(self, date: str | None = None) -> dict | None:
        date = date or self._today_str()
        return await self._scheduler.run_blocking(self._load_summary_sync, date)

    def _load_summary_sync(self, date: str) -> dict | None:
        path = self._data_dir / date / "summary.json"
        if not path.exists():
            return None
        return json.loads(path.read_text(encoding="utf-8"))

    async def list_dates(self) -> list[str]:
        return await self._scheduler.run_blocking(self._list_dates_sync)

    def _list_dates_sync(self) -> list[str]:
        if not self._data_dir.exists():
            return []
        dates = []
        for child in sorted(self._data_dir.iterdir(), reverse=True):
            if child.is_dir() and child.name != "sessions":
                dates.append(child.name)
        return dates
