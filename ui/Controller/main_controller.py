from contextlib import contextmanager

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QInputDialog, QMessageBox, QFileDialog

from clients import fs_client
from DataBinder.main_binder import MainBinder
from Controller.share_controller import ShareController


class MainController:
    def __init__(self, view):
        self.view = view
        self.main_binder = MainBinder(view.style(), getattr(view, "username", ""), getattr(view, "root_folder_id", 1))
        self.share_controller = ShareController(view)
        
        self.view.request_context_menu.connect(self.on_context_menu)
        self.view.request_share.connect(self.on_share)
        self.view.request_create_folder.connect(self.on_create_folder)
        self.view.request_upload_file.connect(self.on_upload_file)
        self.view.request_upload_folder.connect(self.on_upload_folder)
        self.view.request_open_folder.connect(self.on_open_folder)
        self.view.request_home.connect(self.on_home)
        self.view.request_back.connect(self.on_back)
        self.view.request_delete.connect(self.on_delete_item)
        self.view.request_rename.connect(self.on_rename_item)
        self.view.request_download.connect(self.on_download_item)

        self.load()

    @contextmanager
    def _loading(self, message: str):
        self.view.set_loading(True, message)
        try:
            yield
        finally:
            self.view.set_loading(False)

    def load(self):
        with self._loading("Đang tải thư mục..."):
            data = self.main_binder.load_data()
        self.reload(data)

    def on_context_menu(self, row):
        self.view.show_context_menu(row)

    def on_share(self, item):
        self.share_controller.open_share(item)

    def on_create_folder(self):
        name, ok = QInputDialog.getText(self.view, "Tạo thư mục", "Tên thư mục:")
        if ok:
            name = name.strip()
            with self._loading("Đang tạo thư mục..."):
                success, msg = self.main_binder.create_folder(name)
            if success:
                QMessageBox.information(self.view, "Tạo thư mục", msg)
            else:
                QMessageBox.warning(self.view, "Tạo thư mục", msg)
            self.reload()

    def on_upload_file(self):
        path, _ = QFileDialog.getOpenFileName(None, "Chọn tệp để tải lên")
        with self._loading("Đang tải lên file..."):
            success, msg = self.main_binder.upload_file(path)
        if success:
            QMessageBox.information(self.view, "Tải lên file", msg)
        elif msg:
            QMessageBox.warning(self.view, "Tải lên file", msg)
        self.reload()

    def on_upload_folder(self):
        root_dir = QFileDialog.getExistingDirectory(None, "Chọn thư mục để tải lên")

        with self._loading("Đang tải lên thư mục..."):
            success, msg = self.main_binder.upload_folder(root_dir)
        if success:
            QMessageBox.information(self.view, "Tải lên thư mục", msg)
        elif msg:
            QMessageBox.warning(self.view, "Tải lên thư mục", msg or "Tải lên thư mục thất bại")
        self.reload()


    def on_open_folder(self, folder_id: int):
        with self._loading("Đang chuyển tới thư mục..."):
            data = self.main_binder.load_data(folder_id, push_to_stack=True)
        self.reload(data)

    def on_back(self):
        prev = self.main_binder.go_back()
        if prev:
            with self._loading("Đang tải thư mục..."):
                data = self.main_binder.load_data(prev, push_to_stack=False)
            self.reload(data)

    def on_home(self):
        with self._loading("Đang về thư mục gốc..."):
            data = self.main_binder.go_home()
        self.reload(data)

    def on_delete_item(self, item):
        with self._loading("Đang xóa mục..."):
            success, msg = self.main_binder.delete_item(item)
        if success:
            QMessageBox.information(self.view, "Xóa", msg)
        else:
            QMessageBox.warning(self.view, "Xóa", msg)
        self.reload()

    def on_rename_item(self, item):
        new_name, ok = QInputDialog.getText(self.view, "Đổi tên", "Tên mới:", text=item.get("name", ""))
        if ok:
            new_name = new_name.strip()
            with self._loading("Đang đổi tên..."):
                success, msg = self.main_binder.rename_item(item, new_name)
            if success:
                QMessageBox.information(self.view, "Đổi tên", msg)
            else:
                QMessageBox.warning(self.view, "Đổi tên", msg)
            self.reload()

    def on_download_item(self, item):
        title = "Chọn thư mục lưu"
        dest_dir = QFileDialog.getExistingDirectory(None, title)
        with self._loading("Đang tải xuống..."):
            success, msg = self.main_binder.download_item(item, dest_dir)
        if success:
            QMessageBox.information(self.view, "Tải xuống", msg)
        else:
            QMessageBox.warning(self.view, "Tải xuống", msg or "Tải xuống thất bại")
        self.reload()

    def reload(self, data=None):
        if data is None:
            data = self.main_binder.load_data()
        if self.main_binder.isDisconnected:
            self._show_disconnect_dialog()
            return
        self.view.set_table_data(data)
        self._update_nav_buttons()

    def _show_disconnect_dialog(self):
        if not self.main_binder.isDisconnected:
            return
        self._disconnect_dialog_open = True
        dlg = QMessageBox(self.view)
        dlg.setWindowTitle("Mất kết nối")
        dlg.setText("Không thể kết nối đến máy chủ.")
        dlg.setIcon(QMessageBox.Warning)
        dlg.setStandardButtons(QMessageBox.Retry)
        dlg.setWindowFlag(Qt.WindowCloseButtonHint, False)
        dlg.setWindowFlag(Qt.WindowContextHelpButtonHint, False)
        retry_btn = dlg.button(QMessageBox.Retry)
        if retry_btn:
            retry_btn.setText("Kết nối lại")
        dlg.setWindowModality(Qt.ApplicationModal)
        dlg.exec_()
        ok = self.main_binder.reconnect()
        if ok: QMessageBox.information(self.view,  "Thành công", "Kết nối lại thành công")
        self.reload()

    def _update_nav_buttons(self):
        self.view.set_back_enabled(len(self.main_binder.parent_stack) > 0)
        self.view.set_home_enabled(self.main_binder.current_folder_id != self.main_binder.root_folder_id)
