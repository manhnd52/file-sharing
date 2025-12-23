import json
from typing import Any, Tuple

from .fs_connect import Client


class FsApi:
    def __init__(self, host: str = "127.0.0.1", port: int = 5555) -> None:
        self.client = Client(host, port)

    def _parse(self, rc: int, resp: str | None) -> Tuple[int, dict[str, Any] | None]:
        if rc != 0 or resp is None:
            return rc, None
        try:
            data = json.loads(resp)
        except json.JSONDecodeError:
            return rc, None
        return rc, data

    def register(self, username: str, password: str):
        rc, resp = self.client.register(username, password)
        return self._parse(rc, resp)

    def login(self, username: str, password: str):
        rc, resp = self.client.login(username, password)
        return self._parse(rc, resp)

    def list_folder(self, folder_id: int = 0):
        rc, resp = self.client.list_folder(folder_id)
        return self._parse(rc, resp)

    def mkdir(self, parent_id: int, name: str):
        rc, resp = self.client.mkdir(parent_id, name)
        return self._parse(rc, resp)

    # High-level upload/download API (chưa gắn UI đầy đủ, nhưng đã sẵn sàng).
    def upload_init(self, path: str, file_size: int, chunk_size: int):
        rc, resp = self.client.upload_init(path, file_size, chunk_size)
        return self._parse(rc, resp)

    def download(self, path: str):
        rc, resp = self.client.download(path)
        return self._parse(rc, resp)
