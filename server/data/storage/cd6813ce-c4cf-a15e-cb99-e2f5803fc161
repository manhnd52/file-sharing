from PyQt5.QtWidgets import (
    QDialog,
    QVBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QPushButton,
    QHBoxLayout,
    QComboBox,
)


class ShareWindow(QDialog):
    def __init__(self, item_name: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f'Chia sẻ "{item_name}"')
        self.setFixedSize(320, 300)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(10)

        layout.addWidget(QLabel("Thêm người"))
        self.user_input = QLineEdit()
        layout.addWidget(self.user_input)
        self.perm_select = QComboBox()
        self.perm_select.addItem("Chỉ đọc", 1)
        self.perm_select.addItem("Đọc/ghi", 2)
        layout.addWidget(self.perm_select)

        layout.addWidget(QLabel("Những người có quyền truy cập"))
        self.list = QListWidget()
        layout.addWidget(self.list)

        btns = QHBoxLayout()
        btns.addStretch()
        ok = QPushButton("Xong")
        ok.clicked.connect(self.accept)
        btns.addWidget(ok)

        layout.addLayout(btns)
