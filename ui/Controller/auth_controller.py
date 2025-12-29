from PyQt5.QtCore import QObject, pyqtSignal
from PyQt5.QtWidgets import QMessageBox

from DataBinder.auth_binder import AuthBinder


class LoginController(QObject):
    auth_success = pyqtSignal(str, int, str)

    def __init__(self, view):
        super().__init__()
        self.view = view
        self.binder = AuthBinder()

        self.view.login_requested.connect(self.on_login)
        self.view.register_requested.connect(self.on_register)

    def on_login(self, username: str, password: str):
        self.view.setEnabled(False)
        try:
            ok, final_username, msg, root_id, token = self.binder.login(username, password)
        finally:
            self.view.setEnabled(True)

        if ok:
            self.auth_success.emit(final_username, root_id, token)
        else:
            QMessageBox.warning(self.view, "Đăng nhập", msg or "Đăng nhập thất bại")

    def on_register(self, username: str, password: str, confirm: str):
        self.view.setEnabled(False)
        try:
            ok, final_username, msg, root_id, token = self.binder.register(username, password, confirm)
        finally:
            self.view.setEnabled(True)

        if ok:
            self.auth_success.emit(final_username, root_id, token)
        else:
            QMessageBox.warning(self.view, "Đăng ký", msg or "Đăng ký thất bại")
