from __future__ import annotations

import json
from typing import Optional

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import Qt

from .api import FsApi


class ApiWorker(QtCore.QObject):
    finished = QtCore.pyqtSignal(object, object)

    def __init__(self, func, *args) -> None:
        super().__init__()
        self.func = func
        self.args = args

    @QtCore.pyqtSlot()
    def run(self) -> None:
        try:
            rc, data = self.func(*self.args)
        except Exception as e:  # pragma: no cover
            rc, data = -1, {"error": str(e)}
        self.finished.emit(rc, data)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, host: str = "127.0.0.1", port: int = 5555) -> None:
        super().__init__()
        self.api = FsApi(host, port)
        self.current_folder_id: int = 0
        self.logged_in_user: str | None = None

        self.setWindowTitle("File Sharing Client (PyQt)")
        self.resize(1000, 600)

        self._build_ui()

    # ---------- UI building ----------
    def _build_ui(self) -> None:
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        layout = QtWidgets.QVBoxLayout(central)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(12)

        # ===== Top bar: Home | address bar | user info =====
        top_bar = QtWidgets.QHBoxLayout()

        self.btn_home = QtWidgets.QPushButton()
        self.btn_home.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_DirHomeIcon))
        self.btn_home.setIconSize(QtCore.QSize(24, 24))
        self.btn_home.setFlat(True)
        self.btn_home.setCursor(Qt.PointingHandCursor)
        self.btn_home.clicked.connect(self.on_my_folders)

        self.address_bar = QtWidgets.QLineEdit()
        self.address_bar.setReadOnly(True)
        self.address_bar.setPlaceholderText("\\")

        user_layout = QtWidgets.QHBoxLayout()
        self.lbl_username_top = QtWidgets.QLabel("")
        self.lbl_avatar = QtWidgets.QLabel()
        pixmap = self.style().standardPixmap(QtWidgets.QStyle.SP_DirIcon)
        self.lbl_avatar.setPixmap(
            pixmap.scaled(24, 24, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        )

        user_layout.addWidget(self.lbl_username_top)
        user_layout.addWidget(self.lbl_avatar)

        top_bar.addWidget(self.btn_home)
        top_bar.addWidget(self.address_bar, 1)
        top_bar.addSpacing(12)
        top_bar.addLayout(user_layout)

        layout.addLayout(top_bar)

        # ===== Login / status row =====
        top = QtWidgets.QHBoxLayout()

        self.user_edit = QtWidgets.QLineEdit()
        self.user_edit.setPlaceholderText("Username")
        self.pass_edit = QtWidgets.QLineEdit()
        self.pass_edit.setPlaceholderText("Password")
        self.pass_edit.setEchoMode(QtWidgets.QLineEdit.Password)

        self.login_btn = QtWidgets.QPushButton("Login")
        self.login_btn.clicked.connect(self.on_login_clicked)

        self.register_btn = QtWidgets.QPushButton("Register")
        self.register_btn.clicked.connect(self.on_register_clicked)

        self.status_label = QtWidgets.QLabel("Not logged in")

        top.addWidget(self.user_edit)
        top.addWidget(self.pass_edit)
        top.addWidget(self.login_btn)
        top.addWidget(self.register_btn)
        top.addWidget(self.status_label)
        layout.addLayout(top)

        # ===== Toolbar (folders actions) =====
        bar = QtWidgets.QHBoxLayout()
        self.btn_my = QtWidgets.QPushButton("My folders")
        self.btn_shared = QtWidgets.QPushButton("Shared with me")
        self.btn_new_folder = QtWidgets.QPushButton("New folder")

        self.btn_my.clicked.connect(self.on_my_folders)
        self.btn_shared.clicked.connect(self.on_shared_folders)
        self.btn_new_folder.clicked.connect(self.on_new_folder)

        bar.addWidget(self.btn_my)
        bar.addWidget(self.btn_shared)
        bar.addWidget(self.btn_new_folder)
        bar.addStretch(1)
        layout.addLayout(bar)

        # ===== Tree/list view giống mock =====
        self.model = QtGui.QStandardItemModel()
        headers = ["Tên", "Chủ sở hữu", "Ngày sửa đổi", "Kích cỡ tệp", ""]
        self.model.setHorizontalHeaderLabels(headers)

        self.view = QtWidgets.QTreeView()
        self.view.setModel(self.model)
        self.view.setRootIsDecorated(False)
        self.view.setAlternatingRowColors(False)
        self.view.setUniformRowHeights(True)
        self.view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.view.setExpandsOnDoubleClick(False)
        self.view.doubleClicked.connect(self.on_item_double_clicked)

        header = self.view.header()
        header.setSectionResizeMode(0, QtWidgets.QHeaderView.Stretch)
        header.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(2, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(3, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(4, QtWidgets.QHeaderView.Fixed)
        self.view.setColumnWidth(4, 30)

        self.view.setContextMenuPolicy(Qt.CustomContextMenu)
        self.view.customContextMenuRequested.connect(self.on_context_menu)

        layout.addWidget(self.view)

    # ---------- Helpers ----------
    def _set_status(self, text: str) -> None:
        self.status_label.setText(text)

    def _error_box(self, msg: str) -> None:
        QtWidgets.QMessageBox.warning(self, "Error", msg)

    def _reload_current(self) -> None:
        self._load_folder_async(self.current_folder_id)

    def _populate_from_list_resp(self, data: dict) -> None:
        self.model.removeRows(0, self.model.rowCount())
        icon_folder = self.style().standardIcon(QtWidgets.QStyle.SP_DirIcon)
        icon_file = self.style().standardIcon(QtWidgets.QStyle.SP_FileIcon)

        items = data.get("items") or []
        for item in items:
            name = item.get("name", "")
            item_type = item.get("type", "")
            id_ = item.get("id", 0)
            size = item.get("size", "-")

            name_item = QtGui.QStandardItem(name)
            name_item.setIcon(icon_folder if item_type == "folder" else icon_file)
            name_item.setData(id_, role=QtCore.Qt.UserRole + 1)
            name_item.setData(item_type, role=QtCore.Qt.UserRole + 2)

            owner = item.get("owner") or (self.logged_in_user or "")
            owner_item = QtGui.QStandardItem(owner)
            date_item = QtGui.QStandardItem(item.get("modified") or "")
            size_text = "-" if item_type == "folder" else str(size)
            size_item = QtGui.QStandardItem(size_text)
            opt_item = QtGui.QStandardItem("⋮")
            opt_item.setTextAlignment(Qt.AlignCenter)

            self.model.appendRow(
                [name_item, owner_item, date_item, size_item, opt_item]
            )

    # ---------- Slots ----------
    def _run_api_async(self, func, on_done, *args) -> None:
        thread = QtCore.QThread(self)
        worker = ApiWorker(func, *args)
        worker.moveToThread(thread)

        def handle_finished(rc, data) -> None:
            try:
                on_done(rc, data)
            finally:
                thread.quit()
                thread.wait()
                worker.deleteLater()
                thread.deleteLater()

        worker.finished.connect(handle_finished)
        thread.started.connect(worker.run)
        thread.start()

    def _load_folder_async(self, folder_id: int) -> None:
        def done(rc: int, data: dict | None) -> None:
            if rc != 0 or not data or data.get("status") != "ok":
                self._error_box("Failed to load folder")
                return
            self.current_folder_id = int(data.get("folder_id", 0))
            self._populate_from_list_resp(data)

        self._run_api_async(self.api.list_folder, done, folder_id)

    def on_login_clicked(self) -> None:
        username = self.user_edit.text().strip()
        password = self.pass_edit.text()

        if not username or not password:
            self._error_box("Username and password are required")
            return

        self.login_btn.setEnabled(False)

        def done(rc: int, data: dict | None) -> None:
            self.login_btn.setEnabled(True)
            if rc != 0 or not data or data.get("success") is not True:
                err = data.get("error") if data else "Login failed"
                self._error_box(str(err))
                return
            self.logged_in_user = username
            self._set_status(f"Logged in as {username}")
            self.lbl_username_top.setText(username)
            self.address_bar.setText(f"\\{username}")
            self.current_folder_id = 0
            self._load_folder_async(0)

        self._run_api_async(self.api.login, done, username, password)

    def on_register_clicked(self) -> None:
        username = self.user_edit.text().strip()
        password = self.pass_edit.text()

        if not username or not password:
            self._error_box("Username and password are required for registration")
            return

        self.register_btn.setEnabled(False)

        def done(rc: int, data: dict | None) -> None:
            self.register_btn.setEnabled(True)
            if rc != 0 or not data or data.get("success") is not True:
                err = data.get("error") if data else "Register failed"
                self._error_box(str(err))
                return
            self._set_status(f"Registered user {username}")

        self._run_api_async(self.api.register, done, username, password)

    def on_my_folders(self) -> None:
        # My folders: list root-level children (same as LIST with folder_id=0)
        self.current_folder_id = 0
        self._load_folder_async(0)

    def on_shared_folders(self) -> None:
        # Chưa triển khai view “Shared with me” trong bản PyQt này.
        self._error_box("Shared folders view is not implemented yet.")

    def on_new_folder(self) -> None:
        name, ok = QtWidgets.QInputDialog.getText(self, "New folder", "Folder name:")
        if not ok or not name.strip():
            return

        def done(rc: int, data: dict | None) -> None:
            if rc != 0 or not data or data.get("status") != "ok":
                err = data.get("error") if data else "MKDIR failed"
                self._error_box(str(err))
                return
            self._load_folder_async(self.current_folder_id)

        self._run_api_async(self.api.mkdir, done, self.current_folder_id, name.strip())

    def on_item_double_clicked(self, index: QtCore.QModelIndex) -> None:
        # Luôn lấy item ở cột "Tên"
        name_index = index.sibling(index.row(), 0)
        name_item = self.model.itemFromIndex(name_index)
        if not name_item:
            return
        item_type = name_item.data(QtCore.Qt.UserRole + 2)
        item_id = name_item.data(QtCore.Qt.UserRole + 1)
        if item_type == "folder" and item_id:
            self.current_folder_id = int(item_id)
            self._load_folder_async(self.current_folder_id)

    # ---------- Context menu ----------
    def on_context_menu(self, pos: QtCore.QPoint) -> None:
        index = self.view.indexAt(pos)
        if not index.isValid():
            return

        # Luôn làm việc trên cột "Tên"
        name_index = index.sibling(index.row(), 0)
        name_item = self.model.itemFromIndex(name_index)
        if not name_item:
            return
        item_type = name_item.data(QtCore.Qt.UserRole + 2)
        item_id = name_item.data(QtCore.Qt.UserRole + 1)

        menu = QtWidgets.QMenu(self)
        menu.setStyleSheet(
            """
            QMenu {
                background-color: white;
                border: 1px solid black;
                padding: 5px;
            }
            QMenu::item {
                padding: 5px 20px;
                font-size: 13px;
            }
            QMenu::item:selected {
                background-color: #eee;
                color: black;
            }
            """
        )

        act_download = menu.addAction("Tải xuống")
        act_rename = menu.addAction("Đổi tên")
        menu.addSeparator()
        act_share = menu.addAction("Chia sẻ")
        act_move = menu.addAction("Di chuyển")
        menu.addSeparator()
        act_delete = menu.addAction("Xóa")

        chosen = menu.exec_(self.view.viewport().mapToGlobal(pos))
        if not chosen:
            return

        if chosen == act_download:
            self._action_download(item_type, item_id, name_item.text())
        elif chosen == act_rename:
            self._action_rename(item_type, item_id, name_item.text())
        elif chosen == act_share and item_type == "folder":
            self._action_share_folder(item_id)
        elif chosen == act_move:
            self._error_box("Move action is not implemented yet.")
        elif chosen == act_delete:
            self._action_delete(item_type, item_id, name_item.text())

    def _action_download(self, item_type: str, item_id: int, name: str) -> None:
        # Hiện server DOWNLOAD đang demo; tạm thời chỉ báo.
        self._error_box("Download is not fully implemented on server yet.")

    def _action_delete(self, item_type: str, item_id: int, name: str) -> None:
        if item_type != "folder":
            self._error_box("Delete for files is not implemented yet.")
            return

        reply = QtWidgets.QMessageBox.question(
            self,
            "Delete",
            f"Bạn chắc chắn muốn xóa thư mục: {name}?",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if reply != QtWidgets.QMessageBox.Yes:
            return

        def done(rc: int, data: dict | None) -> None:
            if rc != 0 or not data or data.get("status") != "ok":
                err = data.get("error") if data else "Delete failed"
                self._error_box(str(err))
                return
            self._reload_current()

        self._run_api_async(self.api.delete_folder, done, int(item_id))

    def _action_share_folder(self, folder_id: int) -> None:
        username, ok = QtWidgets.QInputDialog.getText(
            self, "Share folder", "Share with username:"
        )
        if not ok or not username.strip():
            return

        perm_str, ok = QtWidgets.QInputDialog.getText(
            self, "Share folder", "Permission (0-3, default 1):"
        )
        if not ok:
            return
        perm = 1
        if perm_str.strip():
            try:
                perm = int(perm_str.strip())
            except ValueError:
                perm = 1

        def done(rc: int, data: dict | None) -> None:
            if rc != 0 or not data or data.get("status") != "ok":
                err = data.get("error") if data else "Share failed"
                self._error_box(str(err))

        self._run_api_async(
            self.api.share_folder, done, int(folder_id), username.strip(), perm
        )

    def _action_rename(self, item_type: str, item_id: int, old_name: str) -> None:
        new_name, ok = QtWidgets.QInputDialog.getText(
            self, "Rename", "New name:", text=old_name
        )
        if not ok or not new_name.strip():
            return

        def done(rc: int, data: dict | None) -> None:
            if rc != 0 or not data or data.get("status") != "ok":
                err = data.get("error") if data else "Rename failed"
                self._error_box(str(err))
                return
            self._reload_current()

        self._run_api_async(
            self.api.rename_item, done, int(item_id), item_type, new_name.strip()
        )
