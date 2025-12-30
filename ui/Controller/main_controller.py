from DataBinder.main_binder import MainBinder
from Controller.share_controller import ShareController
from PyQt5.QtWidgets import QInputDialog, QMessageBox


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
        self.view.request_search.connect(self.on_search)

        self.load()
        self._update_nav_buttons()

    def load(self):
        self.view.setEnabled(False)
        try:
            data = self.main_binder.load_data()
            self.view.set_table_data(data)
        finally:
            self.view.setEnabled(True)
        self._update_nav_buttons()

    def on_context_menu(self, row):
        self.view.show_context_menu(row)

    def on_share(self, item):
        self.share_controller.open_share(item)

    def on_create_folder(self):
        name, ok = QInputDialog.getText(self.view, "Tạo thư mục", "Tên thư mục:")
        if ok:
            name = name.strip()
            success, msg = self.main_binder.create_folder(name)
            if success:
                QMessageBox.information(self.view, "Tạo thư mục", msg)
                self.load()
            else:
                QMessageBox.warning(self.view, "Tạo thư mục", msg)

    def on_upload_file(self):
        success, msg = self.main_binder.upload_file()
        if success:
            QMessageBox.information(self.view, "Tải lên file", msg)
            self.load()
        elif msg:
            QMessageBox.warning(self.view, "Tải lên file", msg)

    def on_upload_folder(self):
        success, msg = self.main_binder.upload_folder()
        if success:
            QMessageBox.information(self.view, "Tải lên thư mục", msg)
            self.load()
        elif msg:
            QMessageBox.warning(self.view, "Tải lên thư mục", msg or "Tải lên thư mục thất bại")


    def on_open_folder(self, folder_id: int):
        data = self.main_binder.load_data(folder_id, push_to_stack=True)
        self.view.set_table_data(data)
        self._update_nav_buttons()

    def on_back(self):
        prev = self.main_binder.go_back()
        if prev:
            data = self.main_binder.load_data(prev, push_to_stack=False)
            self.view.set_table_data(data)
        self._update_nav_buttons()

    def on_home(self):
        data = self.main_binder.go_home()
        self.view.set_table_data(data)
        self._update_nav_buttons()

    def on_delete_item(self, item):
        success, msg = self.main_binder.delete_item(item)
        if success:
            QMessageBox.information(self.view, "Xóa", msg)
            self.load()
        else:
            QMessageBox.warning(self.view, "Xóa", msg)

    def on_rename_item(self, item):
        new_name, ok = QInputDialog.getText(self.view, "Đổi tên", "Tên mới:")
        if ok:
            new_name = new_name.strip()
            success, msg = self.main_binder.rename_item(item, new_name)
            if success:
                QMessageBox.information(self.view, "Đổi tên", msg)
                self.load()
            else:
                QMessageBox.warning(self.view, "Đổi tên", msg)

    def on_download_item(self, item):
        success, msg = self.main_binder.download_item(item)
        if success:
            QMessageBox.information(self.view, "Tải xuống", msg)
        else:
            QMessageBox.warning(self.view, "Tải xuống", msg or "Tải xuống thất bại")

    def _update_nav_buttons(self):
        self.view.set_back_enabled(len(self.main_binder.parent_stack) > 0)
        self.view.set_home_enabled(self.main_binder.current_folder_id != self.main_binder.root_folder_id)

    def on_search(self, keyword: str):
        filtered = self.main_binder.search(keyword)
        self.view.set_table_data(filtered)
