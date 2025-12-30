import json

from clients import fs_client


class AuthBinder:
    """
    Data binder cho phần Auth.
    Gọi client C qua ctypes để đăng nhập / đăng ký.
    Trả về (ok, username, message) để controller hiển thị.
    """

    def login(self, username: str, password: str) -> tuple[bool, str, str, int, str]:
        username = username.strip()
        if not username or not password:
            return False, "", "Thiếu tên đăng nhập hoặc mật khẩu", 0, ""
        ok, resp = fs_client.login(username, password)
        if not ok:
            return False, "", self._extract_error(resp, "Đăng nhập thất bại"), 0, ""
        try:
            data = json.loads(resp) if resp else {}
            return True, data.get("username", username), "", int(data.get("root_folder_id", 0)), data.get("token", "")
        except json.JSONDecodeError:
            return True, username, "", 0, ""

    def register(self, username: str, password: str, confirm: str) -> tuple[bool, str, str, int, str]:
        username = username.strip()
        if not username or not password or password != confirm:
            return False, "", "Thông tin đăng ký không hợp lệ", 0, ""
        ok, resp = fs_client.register(username, password)
        if not ok:
            return False, "", self._extract_error(resp, "Đăng ký thất bại"), 0, ""
        try:
            data = json.loads(resp) if resp else {}
        except json.JSONDecodeError:
            data = {}
        # Đăng nhập ngay sau khi đăng ký để lấy token/root_folder_id
        ok_login, resp_login = fs_client.login(username, password)
        if not ok_login:
            return False, "", self._extract_error(resp_login, "Đăng nhập sau đăng ký thất bại"), 0, ""
        try:
            data_login = json.loads(resp_login) if resp_login else {}
        except json.JSONDecodeError:
            data_login = {}
        root_id = int(data_login.get("root_folder_id", 0))
        return True, data.get("username", username), "", root_id, data_login.get("token", "")

    def _extract_error(self, resp: str, default_msg: str) -> str:
        try:
            data = json.loads(resp) if resp else {}
            return data.get("error", default_msg)
        except json.JSONDecodeError:
            return default_msg
