from __future__ import annotations

import json
from typing import Optional

from PyQt5 import QtCore, QtGui, QtWidgets

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

        self.setWindowTitle("File Sharing Client (PyQt)")
        self.resize(900, 600)

        self._build_ui()

    # ---------- UI building ----------
    def _build_ui(self) -> None:
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        layout = QtWidgets.QVBoxLayout(central)

        # Top: login / status
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

        # Middle: toolbar
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

        # Tree/list view
        self.model = QtGui.QStandardItemModel(0, 2)
        self.model.setHorizontalHeaderLabels(["Name", "Type"])

        self.view = QtWidgets.QTreeView()
        self.view.setModel(self.model)
        self.view.doubleClicked.connect(self.on_item_double_clicked)
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
        items = data.get("items") or []
        for item in items:
            name = item.get("name", "")
            item_type = item.get("type", "")
            id_ = item.get("id", 0)

            name_item = QtGui.QStandardItem(name)
            type_item = QtGui.QStandardItem(item_type)
            name_item.setData(id_, role=QtCore.Qt.UserRole + 1)
            name_item.setData(item_type, role=QtCore.Qt.UserRole + 2)
            self.model.appendRow([name_item, type_item])

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
            self._set_status(f"Logged in as {username}")
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
        name_item = self.model.itemFromIndex(index)
        if not name_item:
            return
        item_type = name_item.data(QtCore.Qt.UserRole + 2)
        item_id = name_item.data(QtCore.Qt.UserRole + 1)
        if item_type == "folder" and item_id:
            self.current_folder_id = int(item_id)
            self._load_folder_async(self.current_folder_id)

    # (Context menu cho delete/share/rename đã được bỏ
    #  để giao diện đơn giản, bám theo API client mới.)
