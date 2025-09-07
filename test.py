from contextlib import contextmanager
from multiprocessing import Process
from multiprocessing.shared_memory import SharedMemory
from time import perf_counter
from typing import Callable
from ipc_semaphore import BusyWaitSemaphore, PosixSemaphore, O_CREAT, O_EXCL


@contextmanager
def BusyWaitSemaphoreContext(owner: bool, index: int):
    name = f"test-busywait-{index}"
    if owner:
        required_mem_size = BusyWaitSemaphore.get_required_memory_size()
        shm = SharedMemory(name=name, create=True, size=required_mem_size)
    else:
        shm = SharedMemory(name=name)
    try:
        sem = BusyWaitSemaphore(shm.buf, owner)
        try:
            yield sem
        finally:
            sem.close()
    finally:
        shm.close()
        if owner:
            shm.unlink()


@contextmanager
def PosixSemaphoreContext(owner: bool, index: int):
    name = f"test-posix-{index}"
    if owner:
        sem = PosixSemaphore(name, O_CREAT | O_EXCL, 0)
    else:
        sem = PosixSemaphore(name, 0, 0)
    try:
        yield sem
    finally:
        sem.close()
        if owner:
            sem.unlink()


def _bench(
    ctx_func, first_move: bool, loop: int, *, hook: Callable[[], None] | None = None
) -> float:
    with (
        ctx_func(first_move, 0 if first_move else 1) as sem0,
        ctx_func(first_move, 1 if first_move else 0) as sem1,
    ):
        if hook:
            hook()

        if first_move:
            sem0.acquire()
            sem1.release()
        else:
            sem1.release()
        start_time = perf_counter()
        for _ in range(loop):
            sem0.acquire()
            sem1.release()
        return perf_counter() - start_time


def main() -> None:
    loop = 1000

    for label, context in (
        ("POSIX", PosixSemaphoreContext),
        ("BusyWait", BusyWaitSemaphoreContext),
    ):
        print(f"{label} Semaphore")
        proc = Process(target=_bench, args=(context, False, loop))
        time = _bench(context, True, loop, hook=proc.start)
        print(f"{label}: {time * 1000}ms")
        proc.join()


if __name__ == "__main__":
    main()
