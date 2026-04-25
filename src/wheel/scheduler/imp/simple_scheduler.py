import re
import threading
import time
from datetime import datetime, timedelta

from src.wheel.scheduler.scheduler import JobStatus, Scheduler
from src.wheel.scheduler.scheduler_models import SchedulerConfig


class _JobEntry:
    def __init__(self, name: str, schedule: str, interval: float, callback, args, kwargs):
        self.name = name
        self.schedule = schedule
        self.interval = interval
        self.callback = callback
        self.args = args
        self.kwargs = kwargs
        self.next_run = time.time() + interval
        self._running = False

    def fire(self):
        try:
            self._running = True
            self.callback(*self.args, **self.kwargs)
        finally:
            self._running = False


class SimpleScheduler(Scheduler):
    def __init__(self, config: SchedulerConfig | None = None):
        self._config = config or SchedulerConfig()
        self._jobs: dict[str, _JobEntry] = {}
        self._running = False
        self._lock = threading.RLock()
        self._loop_thread: threading.Thread | None = None
        self._started = False

    @property
    def name(self) -> str:
        return "SimpleScheduler"

    async def start(self) -> None:
        with self._lock:
            if self._started:
                return
            self._started = True
            self._running = True
            self._loop_thread = threading.Thread(target=self._loop, daemon=True)
            self._loop_thread.start()

    async def stop(self) -> None:
        with self._lock:
            self._running = False
            self._started = False
            self._jobs.clear()
        if self._loop_thread is not None:
            self._loop_thread.join(timeout=2)

    def health_check(self) -> bool:
        with self._lock:
            return self._running and self._loop_thread is not None and self._loop_thread.is_alive()

    def add_job(self, name: str, schedule: str, callback, *args, **kwargs) -> None:
        interval = self._parse_schedule(schedule)
        entry = _JobEntry(name, schedule, interval, callback, args, kwargs)
        with self._lock:
            self._jobs[name] = entry

    def remove_job(self, name: str) -> None:
        with self._lock:
            self._jobs.pop(name, None)

    def get_jobs(self) -> list[JobStatus]:
        with self._lock:
            now = time.time()
            return [
                JobStatus(
                    name=job.name,
                    schedule=job.schedule,
                    next_run=(datetime.now() + timedelta(seconds=max(0, job.next_run - now))).isoformat(),
                    running=job._running,
                )
                for job in self._jobs.values()
            ]

    def _loop(self) -> None:
        semaphore = threading.Semaphore(self._config.max_workers)
        while self._running:
            with self._lock:
                now = time.time()
                nearest = now + 1.0
                for entry in list(self._jobs.values()):
                    if now >= entry.next_run:
                        if entry._running:
                            continue
                        entry.next_run = now + entry.interval
                        t = threading.Thread(target=self._fire_entry, args=(entry, semaphore), daemon=True)
                        t.start()
                    nearest = min(nearest, entry.next_run)
            time.sleep(min(0.1, max(0, nearest - time.time())))

    @staticmethod
    def _fire_entry(entry: _JobEntry, semaphore: threading.Semaphore) -> None:
        semaphore.acquire()
        try:
            entry.fire()
        finally:
            semaphore.release()

    @staticmethod
    def _parse_schedule(schedule: str) -> float:
        schedule = schedule.strip().lower()
        m = re.match(r"^every\s+([\d.]+)\s*(s|sec|secs|second|seconds)$", schedule)
        if m:
            return float(m.group(1))
        m = re.match(r"^every\s+([\d.]+)\s*(m|min|mins|minute|minutes)$", schedule)
        if m:
            return float(m.group(1)) * 60
        m = re.match(r"^every\s+([\d.]+)\s*(h|hr|hrs|hour|hours)$", schedule)
        if m:
            return float(m.group(1)) * 3600
        m = re.match(r"^every\s+([\d.]+)\s*(d|day|days)$", schedule)
        if m:
            return float(m.group(1)) * 86400
        raise ValueError(f"Unsupported schedule format: {schedule}")
