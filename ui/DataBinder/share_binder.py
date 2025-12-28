from clients import fs_client
import json


class ShareBinder:
    """
    Data binder cho chức năng chia sẻ.
    """

    def share_item(self, item: dict, target_username: str, permission: int = 1) -> tuple[bool, str]:
        if not item or item.get("id", 0) <= 0 or not target_username:
            return False, "Thiếu thông tin chia sẻ"
        if item.get("is_folder"):
            ok, resp = fs_client.share_folder(item["id"], target_username, permission)
        else:
            ok, resp = fs_client.share_file(item["id"], target_username, permission)
        if not ok:
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Chia sẻ thất bại")
            except json.JSONDecodeError:
                return False, "Chia sẻ thất bại"
        return True, "Chia sẻ thành công"

    def list_permissions(self, item: dict) -> list:
        if not item or item.get("id", 0) <= 0:
            return []
        target_type = 1 if item.get("is_folder") else 0
        ok, resp = fs_client.list_permissions(target_type, item["id"])
        if not ok:
            return []
        try:
            data = json.loads(resp) if resp else {}
            return data.get("permissions", [])
        except json.JSONDecodeError:
            return []

    def update_permission(self, item: dict, username: str, permission: int) -> tuple[bool, str]:
        if not item or item.get("id", 0) <= 0 or not username:
            return False, "Thiếu dữ liệu"
        target_type = 1 if item.get("is_folder") else 0
        ok, resp = fs_client.update_permission(target_type, item["id"], username, permission)
        if not ok:
            try:
                data = json.loads(resp) if resp else {}
                return False, data.get("error", "Cập nhật quyền thất bại")
            except json.JSONDecodeError:
                return False, "Cập nhật quyền thất bại"
        return True, "Cập nhật quyền thành công"
