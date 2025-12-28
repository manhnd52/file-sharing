from PyQt5.QtCore import QObject

from DataBinder.share_binder import ShareBinder
from Window.share_window import ShareWindow
from PyQt5.QtWidgets import QMessageBox


class ShareController(QObject):
    def __init__(self, view):
        super().__init__()
        self.view = view
        self.binder = ShareBinder()

    def open_share(self, item) -> bool:
        item_name = item.get("name", "") if isinstance(item, dict) else str(item)
        dlg = ShareWindow(item_name, self.view)
        # Load existing permissions
        perms = self.binder.list_permissions(item)
        for p in perms:
            uname = p.get("username", "")
            perm = p.get("permission", 0)
            label = f"{uname} - {'Ghi' if perm >= 2 else 'Đọc'}"
            dlg.list.addItem(label)

        if dlg.exec_():
            new_user = dlg.user_input.text().strip()
            perm_val = dlg.perm_select.currentData()
            self.view.setEnabled(False)
            try:
                ok, msg = self.binder.update_permission(item, new_user, perm_val) if new_user else (False, "Thiếu người dùng")
            finally:
                self.view.setEnabled(True)

            if ok:
                QMessageBox.information(self.view, "Chia sẻ", msg)
            else:
                QMessageBox.warning(self.view, "Chia sẻ", msg)
            return ok

        return False
