import json
import os

from PyQt5.QtWidgets import QStyle, QFileDialog
from clients.fs_client import RequestResult
from clients import fs_client
from session_store import load_session, save_session, clear_session
import json

class MainBinder:
    def __init__(self, style, username: str = "", root_folder_id: int = 1):
        self.style = style
        self.username = username
        self.current_folder_id = root_folder_id or 1
        self.root_folder_id = root_folder_id or 1
        self.parent_stack = []
        self.isDisconnected = False
        self.isError = False
        self.error_message = ""

    def _format_size(self, size):
        if size is None:
            return "-"
        try:
            size = float(size)
        except (TypeError, ValueError):
            return "-"
        units = ["B", "KB", "MB", "GB", "TB"]
        idx = 0
        while size >= 1024 and idx < len(units) - 1:
            size /= 1024.0
            idx += 1
        return f"{size:.1f} {units[idx]}"

    def load_data(self, folder_id: int = None, reset_stack: bool = False, push_to_stack: bool = True):
        if folder_id is None:
            folder_id = self.current_folder_id
        else:
            if reset_stack:
                self.parent_stack = []
            elif push_to_stack and folder_id != self.current_folder_id:
                self.parent_stack.append(self.current_folder_id)
        self.current_folder_id = folder_id
        icon_folder = self.style.standardIcon(QStyle.SP_DirIcon)
        icon_file = self.style.standardIcon(QStyle.SP_FileIcon)

        request_result, resp = fs_client.list_folder(folder_id)
        
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE: self.isDisconnected = True
            return []
        
        try:
            payload = json.loads(resp) if resp else {}
        except json.JSONDecodeError:
            return []

        folder_perm = payload.get("permission", 2)
        base_owner = payload.get("owner_name") or self.username or str(payload.get("owner_id", ""))

        items = payload.get("items", [])
        # If at root, append shared folders/files
        if folder_id == self.root_folder_id:
            ok_shared, resp_shared = fs_client.list_shared_items()
            if ok_shared:
                try:
                    shared_payload = json.loads(resp_shared) if resp_shared else {}
                    for sf in shared_payload.get("items", shared_payload.get("folders", [])):
                        is_folder = sf.get("target_type", 1) == 1
                        sf_item = {
                            "id": sf.get("id"),
                            "name": sf.get("name", ""),
                            "owner": sf.get("owner_name") or str(sf.get("owner_id", "")),
                            "time": "",
                            "size": "-" if is_folder else self._format_size(sf.get("size")),
                            "is_folder": is_folder,
                            "type": "folder" if is_folder else "file",
                            "icon": icon_folder if is_folder else icon_file,
                            "is_shared": True,
                            "permission": sf.get("permission", 0),
                        }
                        items.append(sf_item)
                except json.JSONDecodeError:
                    pass
        data = []
        for item in items:
            is_folder = item.get("type") == "folder"
            owner_val = item.get("owner") or item.get("owner_name") or base_owner
            owner_display = owner_val if item.get("is_shared") else base_owner
            perm_val = item.get("permission", folder_perm)
            is_shared = item.get("is_shared", folder_perm < 2)
            data.append({
                "id": item.get("id"),
                "name": item.get("name", ""),
                "owner": owner_display,
                "time": "",
                "size": "-" if is_folder else self._format_size(item.get("size")),
                "is_folder": is_folder,
                "icon": icon_folder if is_folder else icon_file,
                "is_shared": is_shared,
                "permission": perm_val,
            })
        return data

    def go_back(self):
        if self.parent_stack:
            return self.parent_stack.pop()
        return None

    def go_home(self):
        self.parent_stack = []
        self.current_folder_id = self.root_folder_id
        return self.load_data(self.root_folder_id, reset_stack=True, push_to_stack=False)

    def create_folder(self, name: str) -> tuple[bool, str]:
        if not name:
            return False, "Tên thư mục không được để trống"
        request_result, resp = fs_client.mkdir(self.current_folder_id, name)
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE: self.isDisconnected = True
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Tạo thư mục thất bại")
            except json.JSONDecodeError:
                return False, "Tạo thư mục thất bại"
        return True, "Tạo thư mục thành công"

    def delete_item(self, item) -> tuple[bool, str]:
        if not item:
            return False, "Không có mục để xóa"
        if item.get("is_folder"):
            request_result, resp = fs_client.delete_folder(item.get("id", 0))
        else:
            request_result, resp = fs_client.delete_file(item.get("id", 0))
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE: self.isDisconnected = True
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Xóa thất bại")
            except json.JSONDecodeError:
                return False, "Xóa thất bại"
        return True, "Xóa thành công"

    def rename_item(self, item, new_name: str) -> tuple[bool, str]:
        if not item or not new_name:
            return False, "Thiếu thông tin đổi tên"
        if item.get("is_folder"):
            request_result, resp = fs_client.rename_folder(item.get("id", 0), new_name)
        else:
            request_result, resp = fs_client.rename_file(item.get("id", 0), new_name)
            
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE: self.isDisconnected = True
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Đổi tên thất bại")
            except json.JSONDecodeError:
                return False, "Đổi tên thất bại"
        return True, "Đổi tên thành công"

    def upload_file(self, path) -> tuple[bool, str]:
        if not path:
            return False, ""
        request_result, resp = fs_client.upload_file(path, self.current_folder_id)
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE:
                self.isDisconnected = True
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Tải lên thất bại")
            except json.JSONDecodeError:
                return False, "Tải lên thất bại"
        return True, "Tải lên thành công"

    def _ensure_subfolder(self, parent_id: int, name: str) -> int:
        request_result, resp = fs_client.list_folder(parent_id)
        if request_result == RequestResult.OK:
            try:
                payload = json.loads(resp) if resp else {}
                for item in payload.get("items", []):
                    if item.get("type") == "folder" and item.get("name") == name:
                        return int(item.get("id", 0))
            except json.JSONDecodeError:
                pass
        request_result, resp = fs_client.mkdir(parent_id, name)
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE:
                self.isDisconnected = True
            return 0
        try:
            payload = json.loads(resp) if resp else {}
            return int(payload.get("id", 0))
        except json.JSONDecodeError:
            return 0

    def upload_folder(self, root_dir) -> tuple[bool, str]:
        if not root_dir:
            return False, ""
        root_name = os.path.basename(root_dir.rstrip(os.sep))
        if not root_name:
            return False, "Không xác định được tên thư mục gốc"
        dest_root_id = self._ensure_subfolder(self.current_folder_id, root_name)
        if dest_root_id <= 0:
            return False, f"Tạo thư mục {root_name} thất bại"

        folder_map = {root_dir: dest_root_id}
        for current_path, dirs, files in os.walk(root_dir):
            parent_id = folder_map.get(current_path)
            if not parent_id:
                return False, f"Không tìm được thư mục đích cho {current_path}"
            for d in dirs:
                target_id = self._ensure_subfolder(parent_id, d)
                if target_id <= 0:
                    return False, f"Tạo thư mục {d} thất bại"
                folder_map[os.path.join(current_path, d)] = target_id
            for f in files:
                full_path = os.path.join(current_path, f)
                request_result, resp = fs_client.upload_file(full_path, parent_id)
                if request_result != RequestResult.OK:
                    if request_result == RequestResult.NOT_RESPONSE:
                        self.isDisconnected = True
                    try:
                        data = json.loads(resp) if resp else {}
                        return False, data.get("error", f"Tải lên {f} thất bại")
                    except json.JSONDecodeError:
                        return False, f"Tải lên {f} thất bại"
        return True, "Tải thư mục thành công"

    def download_item(self, item, dest_dir) -> tuple[bool, str]:
        if not item or not item.get("id"):
            return False, "Không có mục để tải xuống"
        is_folder = item.get("is_folder")
        if not dest_dir:
            return False, ""
        if is_folder:
            request_result, resp = fs_client.download_folder(dest_dir, item.get("id"))
        else:
            request_result, resp = fs_client.download_file(dest_dir, item.get("id"))
        if request_result != RequestResult.OK:
            if request_result == RequestResult.NOT_RESPONSE:
                self.isDisconnected = True
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Tải xuống thất bại")
            except json.JSONDecodeError:
                return False, "Tải xuống thất bại"
        return True, "Tải xuống thành công"
    
    def reconnect(self):
        if fs_client.reconnect():
            session = load_session()
            self.isDisconnected = False

            # re-auth
            token = session.get("token")
            ok, resp = fs_client.auth(token)
            if ok:
                try:
                    data = json.loads(resp) if resp else {}
                    username = data.get("username", "")
                    root_id = int(data.get("root_folder_id", 0))
                    save_session(username, token, root_id)
                except json.JSONDecodeError:
                    pass   
                return True   
        else:
            self.isDisconnected = True
            return False