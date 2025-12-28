class ShareBinder:
    """
    Data binder cho chức năng chia sẻ.
    Sau này nối với client/server thật thì chỉ cần sửa lớp này.
    """

    def share_item(self, item_name: str, users: list[str]) -> bool:
        # TODO: gọi API chia sẻ thật ở đây
        # Hiện tại chỉ giả lập là luôn thành công để test luồng.
        return True

