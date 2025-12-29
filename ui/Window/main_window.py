from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLineEdit, QLabel, QTableWidget, QTableWidgetItem,
    QMenu, QAction, QPushButton, QHeaderView, QStyle
)
from PyQt5.QtCore import Qt, QSize, pyqtSignal
from styles import APP_STYLE


class MainWindow(QMainWindow):
    request_context_menu = pyqtSignal(int)
    request_share = pyqtSignal(object)
    request_create_folder = pyqtSignal()
    request_upload_file = pyqtSignal()
    request_open_folder = pyqtSignal(int)
    request_home = pyqtSignal()
    request_back = pyqtSignal()
    request_delete = pyqtSignal(object)
    request_rename = pyqtSignal(object)
    request_logout = pyqtSignal()

    def __init__(self, username: str, root_folder_id: int = 1):
        super().__init__()
        self.username = username
        self.root_folder_id = root_folder_id or 1
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
        self.btn_home.clicked.connect(self._on_home_clicked)

        self.btn_back = QPushButton()
        self.btn_back.setIcon(self.style().standardIcon(QStyle.SP_ArrowBack))
        self.btn_back.setIconSize(QSize(24, 24))
        self.btn_back.setCursor(Qt.PointingHandCursor)
        self.btn_back.setObjectName("BackBtn")
        self.btn_back.clicked.connect(self._on_back_clicked)

        self.path_edit = QLineEdit(f"\\{self.username}")
        self.path_edit.setReadOnly(True)

        self.user_label = QLabel(self.username)
        self.user_label.setObjectName("UserName")

        self.logout_btn = QPushButton("Đăng xuất")
        self.logout_btn.setCursor(Qt.PointingHandCursor)
        self.logout_btn.clicked.connect(self._on_logout_clicked)

        top_layout.addWidget(self.btn_home)
        top_layout.addWidget(self.btn_back)
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
        self.table.itemDoubleClicked.connect(self._on_double_click)
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
        index = self.table.indexAt(pos)
        row = index.row()
        if row >= 0:
            self.request_context_menu.emit(row)
        else:
            self._show_empty_context_menu()

    def show_context_menu(self, row):
        data = self.table.item(row, 0).data(Qt.UserRole)
        perm = data.get("permission", 2)
        can_write = perm >= 2
        can_share = perm >= 2

        menu = QMenu(self)
        download = QAction("Tải xuống", self)
        rename = QAction("Đổi tên", self)
        share = QAction("Chia sẻ", self)
        delete = QAction("Xóa", self)

        menu.addAction(download)
        rename.setEnabled(can_write)
        rename.triggered.connect(lambda: self.request_rename.emit(data))
        menu.addAction(rename)
        share.setEnabled(can_share)
        share.triggered.connect(lambda: self.request_share.emit(data))
        menu.addAction(share)

        menu.addSeparator()
        delete.setEnabled(can_write)
        delete.triggered.connect(lambda: self.request_delete.emit(data))
        menu.addAction(delete)

        menu.exec_(self.cursor().pos())

    def _show_empty_context_menu(self):
        menu = QMenu(self)
        create_folder = QAction("Tạo thư mục mới", self)
        upload_file = QAction("Tải lên tệp", self)
        create_folder.triggered.connect(self.request_create_folder.emit)
        upload_file.triggered.connect(self.request_upload_file.emit)
        menu.addAction(create_folder)
        menu.addAction(upload_file)
        menu.exec_(self.cursor().pos())

    def _on_double_click(self, item):
        row = item.row()
        data = self.table.item(row, 0).data(Qt.UserRole)
        if data and data.get("is_folder"):
            folder_id = data.get("id", 0)
            if folder_id:
                self.request_open_folder.emit(folder_id)

    def _on_back_clicked(self):
        self.request_back.emit()

    def _on_home_clicked(self):
        self.request_home.emit()

    def set_back_enabled(self, enabled: bool):
        self.btn_back.setEnabled(enabled)

    def set_home_enabled(self, enabled: bool):
        self.btn_home.setEnabled(enabled)

    def _apply_style(self):
        self.setStyleSheet(APP_STYLE)

    def _on_logout_clicked(self):
        self.request_logout.emit()
