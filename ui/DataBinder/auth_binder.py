class AuthBinder:
    """
    Data binder cho phần Auth.
    Tạm thời chỉ giả lập login / register để test luồng.
    Sau này nối với client thật (FsApi, REST, ...) chỉ cần sửa class này.
    """

    def login(self, username: str, password: str) -> tuple[bool, str]:
        username = username.strip() or "guest"
        return True, username

    def register(self, username: str, password: str, confirm: str) -> tuple[bool, str]:
        # TODO: sau này thêm validate, gọi API đăng ký thật
        username = username.strip() or "guest"
        return True, username

