from PyQt5.QtWidgets import (
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
)
from PyQt5.QtCore import pyqtSignal, Qt
from styles import APP_STYLE


class LoginWindow(QWidget):
    login_requested = pyqtSignal(str, str)
    register_requested = pyqtSignal(str, str, str)

    def __init__(self):
        super().__init__()
        self.setObjectName("LoginRoot")
        self.is_register_mode = False
        self.setWindowTitle("Đăng nhập Mini Drive")
        # Cùng kích thước với MainWindow
        self.resize(900, 550)
        self._init_ui()
        self._apply_style()

    def _init_ui(self):
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)
        outer.setAlignment(Qt.AlignCenter)

        card = QWidget()
        card.setObjectName("LoginCard")
        card.setFixedWidth(380)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(32, 32, 32, 32)
        layout.setSpacing(14)

        title = QLabel("Mini Drive")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("font-size: 22px; font-weight: bold;")
        layout.addWidget(title)

        self.subtitle = QLabel("Đăng nhập để tiếp tục")
        self.subtitle.setAlignment(Qt.AlignCenter)
        self.subtitle.setStyleSheet("color: #666;")
        layout.addWidget(self.subtitle)

        layout.addSpacing(8)

        user_label = QLabel("Tài khoản (tạm):")
        self.user_input = QLineEdit()
        self.user_input.setPlaceholderText("vd: manhnd52")

        pass_label = QLabel("Mật khẩu (tạm):")
        self.pass_input = QLineEdit()
        self.pass_input.setEchoMode(QLineEdit.Password)
        self.pass_input.setPlaceholderText("nhập bất kỳ để test")

        layout.addWidget(user_label)
        layout.addWidget(self.user_input)
        layout.addWidget(pass_label)
        layout.addWidget(self.pass_input)

        # Trường xác nhận mật khẩu (chỉ dùng ở chế độ đăng ký)
        self.pass_confirm_label = QLabel("Nhập lại mật khẩu:")
        self.pass_confirm_input = QLineEdit()
        self.pass_confirm_input.setEchoMode(QLineEdit.Password)
        layout.addWidget(self.pass_confirm_label)
        layout.addWidget(self.pass_confirm_input)
        self.pass_confirm_label.hide()
        self.pass_confirm_input.hide()

        layout.addSpacing(6)

        btn_row = QHBoxLayout()

        self.toggle_btn = QPushButton("Đăng ký")
        self.toggle_btn.clicked.connect(self._on_toggle_mode)
        btn_row.addWidget(self.toggle_btn)

        btn_row.addStretch()

        self.main_btn = QPushButton("Đăng nhập")
        self.main_btn.clicked.connect(self._on_main_clicked)
        btn_row.addWidget(self.main_btn)

        layout.addLayout(btn_row)
        outer.addWidget(card, alignment=Qt.AlignCenter)

    def _apply_style(self):
        self.setStyleSheet(APP_STYLE)

    def _on_main_clicked(self):
        username = self.user_input.text().strip()
        password = self.pass_input.text()

        if self.is_register_mode:
            confirm = self.pass_confirm_input.text()
            self.register_requested.emit(username, password, confirm)
        else:
            self.login_requested.emit(username, password)

    def _on_toggle_mode(self):
        self.is_register_mode = not self.is_register_mode

        if self.is_register_mode:
            self.subtitle.setText("Đăng ký tài khoản mới")
            self.toggle_btn.setText("Quay lại đăng nhập")
            self.main_btn.setText("Tạo tài khoản")
            self.pass_confirm_label.show()
            self.pass_confirm_input.show()
        else:
            self.subtitle.setText("Đăng nhập để tiếp tục")
            self.toggle_btn.setText("Đăng ký")
            self.main_btn.setText("Đăng nhập")
            self.pass_confirm_label.hide()
            self.pass_confirm_input.hide()

        # Mỗi lần chuyển mode thì xóa dữ liệu cũ
        self.user_input.clear()
        self.pass_input.clear()
        self.pass_confirm_input.clear()
