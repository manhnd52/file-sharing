import os

from PyQt5.QtCore import QObject, QRunnable, QThreadPool, Qt, pyqtSignal
from PyQt5.QtWidgets import QInputDialog, QMessageBox, QFileDialog

from clients import fs_client
from DataBinder.main_binder import MainBinder
from Controller.share_controller import ShareController
from Window.main_window import MainWindow
from Window.download_dialog import DownloadDialog


class _BinderWorkerSignals(QObject):
    finished = pyqtSignal(object)
    error = pyqtSignal(Exception)

class _BinderWorker(QRunnable):
    def __init__(self, func):
        super().__init__()
        self.func = func
        self.signals = _BinderWorkerSignals()

    def run(self):
        try:
            result = self.func()
        except Exception as exc:
            self.signals.error.emit(exc)
        else:
            self.signals.finished.emit(result)


class MainController:
    def __init__(self, view: MainWindow):
        self.view = view
        self.main_binder = MainBinder(view.style(), getattr(view, "username", ""), getattr(view, "root_folder_id", 1))
        self.share_controller = ShareController(view)
        self.thread_pool = QThreadPool()
        self._cache_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "cache.json"))
        self._download_dialog = None
        self._download_target = None  # (item, dest_dir)

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

    def _run_task(self, message: str, func, on_success=None, on_error=None, show_loading: bool = True):
        if show_loading:
            self.view.set_loading(True, message)
        worker = _BinderWorker(func)

        def _done(result):
            if show_loading:
                self.view.set_loading(False)
            self._cancel_context = None
            if on_success:
                on_success(result)

        def _handle_error(exc):
            if show_loading:
                self.view.set_loading(False)
            self._cancel_context = None
            if on_error:
                on_error(exc)
            else:
                QMessageBox.warning(self.view, "Lỗi", str(exc))

        worker.signals.finished.connect(_done)
        worker.signals.error.connect(_handle_error)
        self.thread_pool.start(worker)


    def load(self):
        self._run_task("Đang tải thư mục...", lambda: self.main_binder.load_data(), self.reload)

    def on_context_menu(self, row):
        self.view.show_context_menu(row)

    def on_share(self, item):
        self.share_controller.open_share(item)

    def on_create_folder(self):
        name, ok = QInputDialog.getText(self.view, "Tạo thư mục", "Tên thư mục:")
        if ok:
            name = name.strip()
            def handle(result):
                success, msg = result
                if success:
                    QMessageBox.information(self.view, "Tạo thư mục", msg)
                else:
                    QMessageBox.warning(self.view, "Tạo thư mục", msg)
                self.reload()
            self._run_task("Đang tạo thư mục...", lambda: self.main_binder.create_folder(name), handle)

    def on_upload_file(self):
        path, _ = QFileDialog.getOpenFileName(None, "Chọn tệp để tải lên")
        if not path:
            return
        def handle(result):
            success, msg = result
            if success:
                QMessageBox.information(self.view, "Tải lên file", msg)
            elif msg:
                QMessageBox.warning(self.view, "Tải lên file", msg)
            self.reload()
        self._run_task("Đang tải lên file...", lambda: self.main_binder.upload_file(path), handle)

    def on_upload_folder(self):
        root_dir = QFileDialog.getExistingDirectory(None, "Chọn thư mục để tải lên")
        if not root_dir:
            return
        def handle(result):
            success, msg = result
            if success:
                QMessageBox.information(self.view, "Tải lên thư mục", msg)
            elif msg:
                QMessageBox.warning(self.view, "Tải lên thư mục", msg or "Tải lên thư mục thất bại")
            self.reload()
        self._run_task("Đang tải lên thư mục...", lambda: self.main_binder.upload_folder(root_dir), handle)


    def on_open_folder(self, folder_id: int):
        self._run_task(
            "Đang chuyển tới thư mục...",
            lambda: self.main_binder.load_data(folder_id, push_to_stack=True),
            self.reload,
        )

    def on_back(self):
        prev = self.main_binder.go_back()
        if prev:
            self._run_task(
                "Đang tải thư mục...",
                lambda: self.main_binder.load_data(prev, push_to_stack=False),
                self.reload,
            )

    def on_home(self):
        self._run_task("Đang về thư mục gốc...", lambda: self.main_binder.go_home(), self.reload)

    def on_delete_item(self, item):
        def handle(result):
            success, msg = result
            if success:
                QMessageBox.information(self.view, "Xóa", msg)
            else:
                QMessageBox.warning(self.view, "Xóa", msg)
            self.reload()
        self._run_task("Đang xóa mục...", lambda: self.main_binder.delete_item(item), handle)

    def on_rename_item(self, item):
        new_name, ok = QInputDialog.getText(self.view, "Đổi tên", "Tên mới:", text=item.get("name", ""))
        if ok:
            new_name = new_name.strip()
            def handle(result):
                success, msg = result
                if success:
                    QMessageBox.information(self.view, "Đổi tên", msg)
                else:
                    QMessageBox.warning(self.view, "Đổi tên", msg)
                self.reload()
            self._run_task("Đang đổi tên...", lambda: self.main_binder.rename_item(item, new_name), handle)

    def on_download_item(self, item):
        title = "Chọn thư mục lưu"
        dest_dir = QFileDialog.getExistingDirectory(None, title)
        if not dest_dir:
            return
        self._start_download(item, dest_dir)

    def _start_download(self, item, dest_dir):
        self._download_target = (item, dest_dir)
        dialog = self._ensure_download_dialog()
        dialog.start(item.get("name", ""), dest_dir)
        def handle(result):
            self._handle_download_finished(result)
        self._run_task(
            "Đang tải xuống...",
            lambda: self.main_binder.download_item(item, dest_dir),
            handle,
            self._handle_download_error,
            show_loading=False,
        )

    def _ensure_download_dialog(self):
        if self._download_dialog is None:
            self._download_dialog = DownloadDialog(self._cache_path, self.view)
            self._download_dialog.cancel_requested.connect(self._on_download_cancel_requested)
            self._download_dialog.retry_requested.connect(self._on_download_retry_requested)
            self._download_dialog.closed.connect(self._on_download_dialog_closed)
        return self._download_dialog

    def _handle_download_finished(self, result):
        success, msg = result
        dialog = self._download_dialog
        if self.main_binder.isDisconnected:
            if dialog:
                dialog.show_disconnected(msg or "Mất kết nối đến máy chủ.")
            return
        if dialog:
            dialog.finish(success, msg or ("Tải xuống thành công" if success else "Tải xuống thất bại"))
        if success:
            QMessageBox.information(self.view, "Tải xuống", msg or "Tải xuống thành công")
        elif msg:
            QMessageBox.warning(self.view, "Tải xuống", msg or "Tải xuống thất bại")
        self.reload()

    def _handle_download_error(self, exc: Exception):
        dialog = self._download_dialog
        if dialog:
            dialog.finish(False, str(exc))
        QMessageBox.warning(self.view, "Tải xuống", str(exc))

    def _on_download_cancel_requested(self, session_id: str):
        if not session_id:
            QMessageBox.warning(self.view, "Hủy tải", "Không tìm thấy phiên download hiện tại")
            return
        dialog = self._download_dialog
        def handle(result):
            success, msg = result
            if dialog:
                dialog.show_cancel_result(success, msg or ("Đã gửi yêu cầu hủy" if success else "Hủy download thất bại"))
            if not success and msg:
                QMessageBox.warning(self.view, "Hủy tải", msg)
        self._run_task(
            "Đang hủy download...",
            lambda: self.main_binder.cancel_download_session(session_id),
            handle,
            show_loading=False,
        )

    def _on_download_retry_requested(self):
        dialog = self._download_dialog
        ok = self.main_binder.reconnect()
        if not ok:
            if dialog:
                dialog.show_status("Kết nối lại thất bại", is_error=True)
            QMessageBox.warning(self.view, "Kết nối", "Kết nối lại thất bại")
            return
        if dialog:
            dialog.show_status("Kết nối lại thành công. Đang thử tải lại...", is_error=False)
        if self._download_target:
            item, dest_dir = self._download_target
            dialog.start(item.get("name", ""), dest_dir)
            def handle(result):
                self._handle_download_finished(result)
            self._run_task(
                "Đang tải xuống...",
                lambda: self.main_binder.resume_download_session(),
                handle,
                self._handle_download_error,
                show_loading=False,
            )

    def _on_download_dialog_closed(self):
        self._download_dialog = None
        self._download_target = None

    def reload(self, data=None):
        if data is None:
            self.load()
            return
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
