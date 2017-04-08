from pathlib import Path
from typing import List, Optional
from lib import engine, util


class Artifact:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.stat = path.stat()

    def update_stat(self) -> None:
        self.stat = self.path.stat()

    @property
    def was_changed(self) -> bool:
        return self.path.stat().st_mtime > self.stat.st_mtime


class Snapshot:
    def __init__(self, file_entry: engine.FileEntry) -> None:
        self.entry = file_entry
        self.extra_artifacts = {}  # type: Dict[str, Artifact]
        self.main_artifact = None  # type: Optional[Artifact]

    def save_main_artifact(self, path: Path, content: bytes) -> None:
        util.save_file(path, content)
        self.main_artifact = Artifact(path)

    def save_extra_artifact(
            self, artifact_id: str, path: Path, content: bytes) -> None:
        util.save_file(path, content)
        self.extra_artifacts[artifact_id] = Artifact(path)

    def read_main_artifact(self) -> Optional[bytes]:
        if not self.main_artifact:
            return None
        path = self.main_artifact.path
        if not path.exists():
            return None
        with path.open('rb') as handle:
            return handle.read()

    def read_extra_artifact(self, artifact_id: str) -> Optional[bytes]:
        if artifact_id not in self.extra_artifacts:
            return None
        path = self.extra_artifacts[artifact_id].path
        if not path.exists():
            return None
        with path.open('rb') as handle:
            return handle.read()

    @property
    def all_artifacts(self) -> List[Artifact]:
        return (
            ([self.main_artifact] if self.main_artifact else [])
            + list(self.extra_artifacts.values()))

    @property
    def was_changed(self) -> bool:
        return any(artifact.was_changed for artifact in self.all_artifacts)
