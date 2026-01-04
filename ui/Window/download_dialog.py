import json
import os
from typing import Optional

from PyQt5.QtCore import Qt, QTimer, pyqtSignal
from PyQt5.QtWidgets import (
    QDialog,
    QHBoxLayout,
    QLabel,
    QProgressBar,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class DownloadDialog(QDialog):
    cancel_requested = pyqtSignal(str)
    retry_requested = pyqtSignal()
    closed = pyqtSignal()

    def __init__(self, cache_path: str, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self.cache_path = cache_path
        self._timer = QTimer(self)
        self._timer.setInterval(700)
        self._timer.timeout.connect(self._refresh_from_cache)

        self._current_session = ""
        self._dest_path = ""
        self._file_name = ""

        self._build_ui()

    def _build_ui(self):
        self.setWindowTitle("Đang tải xuống")
        self.setModal(True)
        self.setMinimumWidth(480)
        self.setWindowFlag(Qt.WindowContextHelpButtonHint, False)

        layout = QVBoxLayout(self)
        layout.setSpacing(12)

        title = QLabel("Đang tải xuống")
        title.setStyleSheet("font-size: 16px; font-weight: 600; color: #1a1a1a;")
        layout.addWidget(title)

        info_layout = QVBoxLayout()
        self.file_value = QLabel("-")
        self.file_value.setWordWrap(True)
        self.dest_value = QLabel("-")
        self.dest_value.setWordWrap(True)
        self.session_value = QLabel("Session: Đang khởi tạo...")
        info_layout.addWidget(self._build_row("Tên tệp", self.file_value))
        info_layout.addWidget(self._build_row("Lưu tại", self.dest_value))
        info_layout.addWidget(self.session_value)
        layout.addLayout(info_layout)

        self.progress_bar = QProgressBar()
        self.progress_bar.setMaximum(100)
        self.progress_bar.setValue(0)
        self.progress_bar.setTextVisible(False)
        layout.addWidget(self.progress_bar)

        self.progress_text = QLabel("0% • 0 / 0")
        self.progress_text.setStyleSheet("color: #555;")
        layout.addWidget(self.progress_text)

        self.status_label = QLabel("Đang tải xuống...")
        self.status_label.setStyleSheet("color: #333;")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        self.retry_btn = QPushButton("Thử lại")
        self.retry_btn.hide()
        self.retry_btn.clicked.connect(self._on_retry_clicked)
        self.cancel_btn = QPushButton("Hủy tải")
        self.cancel_btn.clicked.connect(self._on_cancel_clicked)
        self.close_btn = QPushButton("Đóng")
        self.close_btn.setEnabled(False)
        self.close_btn.clicked.connect(self.close)
        btn_layout.addWidget(self.retry_btn)
        btn_layout.addWidget(self.cancel_btn)
        btn_layout.addWidget(self.close_btn)
        layout.addLayout(btn_layout)

    def _build_row(self, label_text: str, value_widget: QWidget) -> QWidget:
        container = QWidget()
        layout = QHBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        label = QLabel(label_text)
        label.setStyleSheet("color: #444; font-weight: 500;")
        layout.addWidget(label, 0)
        layout.addWidget(value_widget, 1)
        return container

    def start(self, file_name: str, dest_path: str):
        self._current_session = ""
        self._file_name = file_name or ""
        self._dest_path = dest_path or ""
        self.file_value.setText(self._file_name or "-")
        self.dest_value.setText(self._dest_path or "-")
        self.session_value.setText("Session: Đang khởi tạo...")
        self.progress_bar.setMaximum(100)
        self.progress_bar.setValue(0)
        self.progress_text.setText("0% • 0 / 0")
        self.show_status("Đang tải xuống...", is_error=False)
        self.retry_btn.hide()
        self.cancel_btn.setEnabled(True)
        self.close_btn.setEnabled(False)
        self._timer.start()
        self._refresh_from_cache()
        self.show()

    def show_status(self, message: str, is_error: bool = False):
        if not message:
            return
        color = "#d32f2f" if is_error else "#333"
        self.status_label.setStyleSheet(f"color: {color};")
        self.status_label.setText(message)

    def finish(self, success: bool, message: str = ""):
        self._timer.stop()
        if success:
            self.progress_bar.setMaximum(100)
            self.progress_bar.setValue(100)
        self.retry_btn.hide()
        self.cancel_btn.setEnabled(False)
        self.close_btn.setEnabled(True)
        self.show_status(message or ("Tải xuống thành công" if success else "Tải xuống thất bại"), is_error=not success)

    def show_disconnected(self, message: str = ""):
        self._timer.stop()
        self.retry_btn.show()
        self.cancel_btn.setEnabled(False)
        self.close_btn.setEnabled(True)
        self.show_status(message or "Mất kết nối tới máy chủ", is_error=True)

    def show_cancel_result(self, success: bool, message: str = ""):
        self.show_status(message or ("Đã gửi yêu cầu hủy" if success else "Hủy download thất bại"), is_error=not success)
        if success:
            self.cancel_btn.setEnabled(False)
        else:
            self.cancel_btn.setEnabled(True)

    def _on_cancel_clicked(self):
        self.cancel_btn.setEnabled(False)
        self.show_status("Đang gửi yêu cầu hủy...", is_error=False)
        self.cancel_requested.emit(self._current_session)

    def _on_retry_clicked(self):
        self.retry_btn.setEnabled(False)
        self.show_status("Đang thử lại...", is_error=False)
        self.retry_requested.emit()
        self.retry_btn.setEnabled(True)

    def _refresh_from_cache(self):
        data = self._read_cache()
        if not data:
            return
        node = data.get("downloading") or {}
        if not isinstance(node, dict):
            return

        session_id = str(node.get("session_id") or "").strip()
        if session_id:
            self._current_session = session_id
            self.session_value.setText(f"Session: {session_id}")

        file_name = node.get("file_name") or self._file_name
        if file_name:
            self._file_name = file_name
            self.file_value.setText(file_name)

        storage_path = node.get("storage_path") or self._dest_path
        if storage_path:
            self._dest_path = storage_path
            self.dest_value.setText(storage_path)

        total_size = self._to_int(node.get("total_size"))
        chunk_size = self._to_int(node.get("chunk_size"))
        last_chunk = self._to_int(node.get("last_received_chunk"))
        received = max(0, last_chunk) * max(0, chunk_size)

        if total_size > 0:
            percent = min(100, int(received * 100 / total_size))
            self.progress_bar.setMaximum(100)
            self.progress_bar.setValue(percent)
        else:
            percent = 0
            self.progress_bar.setMaximum(0)
        self.progress_text.setText(f"{percent}% • {self._format_size(received)} / {self._format_size(total_size)}")

    def _read_cache(self):
        if not self.cache_path or not os.path.exists(self.cache_path):
            return {}
        try:
            with open(self.cache_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except (OSError, json.JSONDecodeError):
            return {}

    def _to_int(self, value) -> int:
        try:
            return int(value)
        except (TypeError, ValueError):
            return 0

    def _format_size(self, size: int) -> str:
        if not size or size <= 0:
            return "0 B"
        units = ["B", "KB", "MB", "GB", "TB"]
        idx = 0
        size_f = float(size)
        while size_f >= 1024 and idx < len(units) - 1:
            size_f /= 1024.0
            idx += 1
        return f"{size_f:.1f} {units[idx]}"

    def closeEvent(self, event):
        self._timer.stop()
        self.closed.emit()
        super().closeEvent(event)