from Window.auth_window import LoginWindow
from Controller.main_controller import MainController
from Controller.auth_controller import LoginController
from Window.main_window import MainWindow


class AppController:
    """
    Controller tổng: điều hướng giữa LoginWindow và MainWindow.
    Các controller con (LoginController, MainController) chỉ lo logic của từng page.
    """

    def __init__(self):
        self.login_view = LoginWindow()
        self.login_controller = LoginController(self.login_view)
        self.main_view = None
        self.main_controller = None
        self._last_pos = None
        self.login_controller.auth_success.connect(self._on_auth_success)

    def start(self):
        self.login_view.show()

    def _on_auth_success(self, username: str, root_id: int):
        self._last_pos = self.login_view.pos()
        self.login_view.hide()
        self.main_view = MainWindow(username, root_id)
        if self._last_pos is not None:
            self.main_view.move(self._last_pos)
        self.main_controller = MainController(self.main_view)
        self.main_view.request_logout.connect(self._on_logout)
        self.main_view.show()

    def _on_logout(self):
        if self.main_view is not None:
            self._last_pos = self.main_view.pos()
            self.main_view.close()
            self.main_view = None
            self.main_controller = None

        if self._last_pos is not None:
            self.login_view.move(self._last_pos)
        self.login_view.show()
