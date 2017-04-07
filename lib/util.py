import threading
from pathlib import Path


_lock = threading.Lock()


def save_file(path: Path, content: bytes) -> None:
    with _lock:  # mkdir doesn't seem to be thread safe
        path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('wb') as handle:
        handle.write(content)
