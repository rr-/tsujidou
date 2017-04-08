from pathlib import Path
from typing import Any, Optional
from lib import engine


class SnapshotEntry:
    def __init__(
            self,
            file_num: int,
            file_type: engine.FileType,
            file_name: Optional[str],
            file_name_hash: int,
            relative_path: Path,
            stat: Any,
            metadata: Any) -> None:
        self.file_num = file_num
        self.file_type = file_type
        self.file_name = file_name
        self.file_name_hash = file_name_hash
        self.relative_path = relative_path
        self.stat = stat
        self.metadata = metadata  # metadata associated with file conversion
