import asyncio
import threading
import time

import pytest

from src.wheel.scheduler.imp.simple_scheduler import SimpleScheduler
from src.wheel.scheduler.scheduler_factory import SchedulerFactory
from src.wheel.scheduler.scheduler_models import SchedulerConfig


class TestSimpleSchedulerTA:
    def test_name(self):
        s = SimpleScheduler()
        assert s.name == "SimpleScheduler"

    def test_health_check_before_start(self):
        s = SimpleScheduler()
        assert s.health_check() is False

    @pytest.mark.asyncio
    async def test_full_lifecycle(self):
        s = SimpleScheduler()
        assert s.health_check() is False
        await s.start()
        assert s.health_check() is True
        await s.stop()
        assert s.health_check() is False

    @pytest.mark.asyncio
    async def test_start_idempotent(self):
        s = SimpleScheduler()
        await s.start()
        assert s.health_check() is True
        await s.start()
        assert s.health_check() is True
        await s.stop()

    @pytest.mark.asyncio
    async def test_stop_clears_jobs(self):
        s = SimpleScheduler()
        s.add_job("test", "every 1 hours", lambda: None)
        assert len(s.get_jobs()) == 1
        await s.start()
        await s.stop()
        assert len(s.get_jobs()) == 0

    @pytest.mark.asyncio
    async def test_stop_before_start_no_error(self):
        s = SimpleScheduler()
        await s.stop()
        assert s.health_check() is False

    @pytest.mark.asyncio
    async def test_add_job_before_start(self):
        s = SimpleScheduler()
        s.add_job("test", "every 1 hours", lambda: None)
        jobs = s.get_jobs()
        assert len(jobs) == 1
        assert jobs[0].name == "test"
        assert jobs[0].schedule == "every 1 hours"

    @pytest.mark.asyncio
    async def test_add_job_after_start(self):
        s = SimpleScheduler()
        await s.start()
        s.add_job("test", "every 1 hours", lambda: None)
        jobs = s.get_jobs()
        assert len(jobs) == 1
        await s.stop()

    @pytest.mark.asyncio
    async def test_remove_job(self):
        s = SimpleScheduler()
        s.add_job("a", "every 1 hours", lambda: None)
        s.add_job("b", "every 2 hours", lambda: None)
        assert len(s.get_jobs()) == 2
        s.remove_job("a")
        assert len(s.get_jobs()) == 1
        assert s.get_jobs()[0].name == "b"

    @pytest.mark.asyncio
    async def test_remove_nonexistent_job(self):
        s = SimpleScheduler()
        s.remove_job("no-such")
        assert len(s.get_jobs()) == 0

    @pytest.mark.asyncio
    async def test_add_job_overwrites_same_name(self):
        s = SimpleScheduler()
        s.add_job("test", "every 1 hours", lambda: 1)
        s.add_job("test", "every 2 hours", lambda: 2)
        jobs = s.get_jobs()
        assert len(jobs) == 1
        assert jobs[0].schedule == "every 2 hours"

    @pytest.mark.asyncio
    async def test_callback_fires(self):
        results = []
        s = SimpleScheduler()
        s.add_job("fire", "every 0.5 seconds", lambda: results.append(1))
        await s.start()
        await asyncio.sleep(1.2)
        await s.stop()
        assert len(results) >= 1

    @pytest.mark.asyncio
    async def test_callback_fires_multiple(self):
        results = []
        s = SimpleScheduler()
        s.add_job("fire", "every 0.3 seconds", lambda: results.append(1))
        await s.start()
        await asyncio.sleep(1.0)
        await s.stop()
        assert len(results) >= 2

    @pytest.mark.asyncio
    async def test_callback_receives_args(self):
        results = []
        s = SimpleScheduler()
        s.add_job("fire", "every 0.3 seconds", lambda x, y: results.append(x + y), 10, 20)
        await s.start()
        await asyncio.sleep(0.5)
        await s.stop()
        assert len(results) >= 1
        assert results[0] == 30

    @pytest.mark.asyncio
    async def test_callback_receives_kwargs(self):
        results = []
        s = SimpleScheduler()
        s.add_job("fire", "every 0.3 seconds", lambda a=0, b=0: results.append(a + b), b=5, a=3)
        await s.start()
        await asyncio.sleep(0.5)
        await s.stop()
        assert len(results) >= 1
        assert results[0] == 8

    @pytest.mark.asyncio
    async def test_multiple_jobs_independent(self):
        results_a = []
        results_b = []
        s = SimpleScheduler()
        s.add_job("a", "every 0.3 seconds", lambda: results_a.append("a"))
        s.add_job("b", "every 0.5 seconds", lambda: results_b.append("b"))
        await s.start()
        await asyncio.sleep(1.2)
        await s.stop()
        assert len(results_a) >= 2
        assert len(results_b) >= 1

    @pytest.mark.asyncio
    async def test_callback_exception_does_not_break_scheduler(self):
        results = []

        def bad():
            raise RuntimeError("boom")

        s = SimpleScheduler()
        s.add_job("bad", "every 0.3 seconds", bad)
        s.add_job("good", "every 0.3 seconds", lambda: results.append(1))
        await s.start()
        await asyncio.sleep(1.0)
        await s.stop()
        assert len(results) >= 2

    @pytest.mark.asyncio
    async def test_callback_running_flag(self):
        started = threading.Event()
        block = threading.Event()

        def slow():
            started.set()
            block.wait()

        s = SimpleScheduler(config=SchedulerConfig(max_workers=2))
        s.add_job("slow", "every 0.1 seconds", slow)
        await s.start()
        started.wait(timeout=2)
        await asyncio.sleep(0.3)
        jobs = s.get_jobs()
        assert jobs[0].running is True
        block.set()
        await s.stop()

    @pytest.mark.asyncio
    async def test_parse_schedule_seconds(self):
        s = SimpleScheduler()
        assert s._parse_schedule("every 10 seconds") == 10
        assert s._parse_schedule("every 1 sec") == 1
        assert s._parse_schedule("every 2.5 s") == 2.5

    @pytest.mark.asyncio
    async def test_parse_schedule_minutes(self):
        s = SimpleScheduler()
        assert s._parse_schedule("every 5 minutes") == 300
        assert s._parse_schedule("every 1 min") == 60
        assert s._parse_schedule("every 2 m") == 120

    @pytest.mark.asyncio
    async def test_parse_schedule_hours(self):
        s = SimpleScheduler()
        assert s._parse_schedule("every 1 hour") == 3600
        assert s._parse_schedule("every 2 hours") == 7200
        assert s._parse_schedule("every 1 h") == 3600

    @pytest.mark.asyncio
    async def test_parse_schedule_days(self):
        s = SimpleScheduler()
        assert s._parse_schedule("every 1 day") == 86400
        assert s._parse_schedule("every 7 days") == 604800

    @pytest.mark.asyncio
    async def test_parse_schedule_case_insensitive(self):
        s = SimpleScheduler()
        assert s._parse_schedule("EVERY 10 SECONDS") == 10
        assert s._parse_schedule("Every 5 Minutes") == 300

    @pytest.mark.asyncio
    async def test_parse_schedule_invalid_raises(self):
        s = SimpleScheduler()
        with pytest.raises(ValueError, match="Unsupported schedule format"):
            s._parse_schedule("at 3pm")
        with pytest.raises(ValueError, match="Unsupported schedule format"):
            s._parse_schedule("")
        with pytest.raises(ValueError, match="Unsupported schedule format"):
            s._parse_schedule("every")

    @pytest.mark.asyncio
    async def test_max_workers_limit(self):
        running_count = 0
        lock = threading.Lock()
        max_seen = 0
        results = []

        def worker():
            nonlocal running_count, max_seen
            with lock:
                running_count += 1
                max_seen = max(max_seen, running_count)
            time.sleep(0.3)
            with lock:
                running_count -= 1
            results.append(1)

        s = SimpleScheduler(config=SchedulerConfig(max_workers=2))
        for i in range(5):
            s.add_job(f"w{i}", "every 0.1 seconds", worker)
        await s.start()
        await asyncio.sleep(1.0)
        await s.stop()
        assert max_seen <= 2
        assert len(results) >= 1

    @pytest.mark.asyncio
    async def test_factory_create_and_run(self):
        s = SchedulerFactory().create(SchedulerConfig(max_workers=1))
        results = []
        s.add_job("t", "every 0.3 seconds", lambda: results.append(1))
        await s.start()
        await asyncio.sleep(0.8)
        await s.stop()
        assert len(results) >= 2

    @pytest.mark.asyncio
    async def test_run_blocking_returns_result(self):
        s = SimpleScheduler()
        await s.start()
        result = await s.run_blocking(lambda x, y: x + y, 10, 20)
        assert result == 30
        await s.stop()

    @pytest.mark.asyncio
    async def test_run_blocking_with_kwargs(self):
        s = SimpleScheduler()
        await s.start()
        result = await s.run_blocking(lambda a=0, b=0: a * b, a=6, b=7)
        assert result == 42
        await s.stop()

    @pytest.mark.asyncio
    async def test_run_blocking_propagates_exception(self):
        s = SimpleScheduler()

        def fail():
            raise ValueError("expected")

        await s.start()
        with pytest.raises(ValueError, match="expected"):
            await s.run_blocking(fail)
        await s.stop()

    @pytest.mark.asyncio
    async def test_run_blocking_respects_max_workers(self):
        running = 0
        max_seen = 0
        lock = threading.Lock()
        results = []

        def slow():
            nonlocal running, max_seen
            with lock:
                running += 1
                max_seen = max(max_seen, running)
            time.sleep(0.3)
            with lock:
                running -= 1
            results.append(1)

        s = SimpleScheduler(config=SchedulerConfig(max_workers=2))
        await s.start()
        tasks = [s.run_blocking(slow) for _ in range(5)]
        await asyncio.gather(*tasks)
        await s.stop()
        assert max_seen <= 2
        assert len(results) == 5

    @pytest.mark.asyncio
    async def test_run_blocking_requires_event_loop(self):
        s = SimpleScheduler()
        await s.start()
        result = await s.run_blocking(lambda: 42)
        assert result == 42
        await s.stop()

    @pytest.mark.asyncio
    async def test_run_blocking_string_result(self):
        s = SimpleScheduler()
        await s.start()
        result = await s.run_blocking(lambda: "hello")
        assert result == "hello"
        await s.stop()

    @pytest.mark.asyncio
    async def test_run_blocking_none_result(self):
        s = SimpleScheduler()
        await s.start()
        result = await s.run_blocking(lambda: None)
        assert result is None
        await s.stop()

