from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLineEdit, QLabel, QTableWidget, QTableWidgetItem,
    QMenu, QAction, QPushButton, QHeaderView, QStyle
)
from PyQt5.QtCore import Qt, QSize, pyqtSignal
from styles import APP_STYLE


class MainWindow(QMainWindow):
    request_context_menu = pyqtSignal(int)
    request_share = pyqtSignal(str)
    request_logout = pyqtSignal()

    def __init__(self, username: str):
        super().__init__()
        self.username = username
        self.setWindowTitle("Mini Drive")
        self.resize(900, 550)

        self._init_ui()
        self._apply_style()

    # ---------- UI ----------
    def _init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)

        layout = QVBoxLayout(central)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(15)

        # Top bar
        top_layout = QHBoxLayout()
        self.btn_home = QPushButton()
        self.btn_home.setIcon(self.style().standardIcon(QStyle.SP_DirHomeIcon))
        self.btn_home.setIconSize(QSize(28, 28))
        self.btn_home.setCursor(Qt.PointingHandCursor)
        self.btn_home.setObjectName("HomeBtn")

        self.path_edit = QLineEdit(f"\\{self.username}")
        self.path_edit.setReadOnly(True)

        self.user_label = QLabel(self.username)
        self.user_label.setObjectName("UserName")

        self.logout_btn = QPushButton("Đăng xuất")
        self.logout_btn.setCursor(Qt.PointingHandCursor)
        self.logout_btn.clicked.connect(self._on_logout_clicked)

        top_layout.addWidget(self.btn_home)
        top_layout.addWidget(self.path_edit)
        top_layout.addStretch()
        top_layout.addWidget(self.user_label)
        top_layout.addWidget(self.logout_btn)

        top = QWidget()
        top.setObjectName("TopBar")
        top.setLayout(top_layout)

        # Table
        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(
            ["Tên", "Chủ sở hữu", "Ngày sửa đổi", "Kích cỡ tệp"]
        )
        self.table.verticalHeader().setVisible(False)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setSelectionBehavior(QTableWidget.SelectRows)
        self.table.setSelectionMode(QTableWidget.SingleSelection)
        self.table.setContextMenuPolicy(Qt.CustomContextMenu)
        self.table.customContextMenuRequested.connect(self._on_right_click)
        self.table.setShowGrid(False)
        self.table.setFocusPolicy(Qt.NoFocus)

        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.Stretch)
        header.setSectionResizeMode(1, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(2, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.ResizeToContents)

        layout.addWidget(top)
        layout.addWidget(self.table)

    # ---------- VIEW API ----------
    def set_table_data(self, data):
        self.table.setRowCount(len(data))
        for row, item in enumerate(data):
            self._set_item(row, 0, item["name"], Qt.AlignVCenter | Qt.AlignLeft, item)
            self._set_item(row, 1, item["owner"], Qt.AlignVCenter | Qt.AlignLeft)
            self._set_item(row, 2, item["time"], Qt.AlignVCenter | Qt.AlignLeft)
            self._set_item(row, 3, item["size"], Qt.AlignVCenter | Qt.AlignRight)

    def _set_item(self, row, col, text, align, data=None):
        item = QTableWidgetItem(text)
        item.setTextAlignment(align)
        if data:
            item.setIcon(data["icon"])
            item.setData(Qt.UserRole, data)
        self.table.setItem(row, col, item)

    # ---------- EVENT ----------
    def _on_right_click(self, pos):
        row = self.table.currentRow()
        if row >= 0:
            self.request_context_menu.emit(row)

    def show_context_menu(self, row):
        data = self.table.item(row, 0).data(Qt.UserRole)

        menu = QMenu(self)
        download = QAction("Tải xuống", self)
        rename = QAction("Đổi tên", self)
        share = QAction("Chia sẻ", self)
        delete = QAction("Xóa", self)

        menu.addAction(download)
        menu.addAction(rename)
        share.triggered.connect(lambda: self.request_share.emit(data["name"]))
        menu.addAction(share)

        menu.addSeparator()
        menu.addAction(delete)

        menu.exec_(self.cursor().pos())

    def _apply_style(self):
        self.setStyleSheet(APP_STYLE)

    def _on_logout_clicked(self):
        self.request_logout.emit()
