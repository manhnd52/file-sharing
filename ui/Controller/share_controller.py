from PyQt5.QtCore import QObject

from DataBinder.share_binder import ShareBinder
from Window.share_window import ShareWindow


class ShareController(QObject):
    def __init__(self, view):
        super().__init__()
        self.view = view
        self.binder = ShareBinder()

    def open_share(self, item_name: str) -> bool:
        dlg = ShareWindow(item_name, self.view)
        if dlg.exec_():
            new_user = dlg.user_input.text().strip()
            users = [new_user] if new_user else []

            self.view.setEnabled(False)
            try:
                ok = self.binder.share_item(item_name, users)
            finally:
                self.view.setEnabled(True)

            return ok

        return False
