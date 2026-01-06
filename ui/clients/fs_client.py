import json
import os
from ctypes import CDLL, c_char_p, c_uint16, c_int, c_size_t, create_string_buffer
from enum import Enum
from typing import Callable, Tuple

DEFAULT_HOST = os.environ.get("FS_HOST", "127.0.0.1")
DEFAULT_PORT = int(os.environ.get("FS_PORT", "5555"))
DEFAULT_TIMEOUT = int(os.environ.get("FS_TIMEOUT", "5"))

def _load_library():
    cand = [
        os.environ.get("FS_CLIENT_LIB"),
        os.path.join(os.path.dirname(__file__), "..", "libfsclient.so"),
        os.path.join(os.path.dirname(__file__), "..", "libclient.so"),
        os.path.join(os.path.dirname(__file__), "libfsclient.so"),
        os.path.join(os.path.dirname(__file__), "libclient.so"),
        os.path.join(os.path.dirname(__file__), "..", "..", "client", "libfsclient.so"),
        os.path.join(os.path.dirname(__file__), "..", "..", "client", "libclient.so"),
        os.path.join(os.getcwd(), "libfsclient.so"),
        os.path.join(os.getcwd(), "libclient.so"),
    ]
    for path in cand:
        if path and os.path.exists(path):
            return CDLL(path)
    raise RuntimeError("Cannot find libfsclient.so; set FS_CLIENT_LIB or place it next to UI.")

lib = _load_library()

lib.fs_connect.argtypes = [c_char_p, c_uint16, c_int]
lib.fs_connect.restype = c_int
lib.fs_disconnect.argtypes = []
lib.fs_disconnect.restype = None

lib.fs_login_json.argtypes = [c_char_p, c_char_p, c_char_p, c_size_t]
lib.fs_login_json.restype = c_int
lib.fs_register_json.argtypes = [c_char_p, c_char_p, c_char_p, c_size_t]
lib.fs_register_json.restype = c_int
lib.fs_auth_json.argtypes = [c_char_p, c_char_p, c_size_t]
lib.fs_auth_json.restype = c_int
lib.fs_logout_json.argtypes = [c_char_p, c_size_t]
lib.fs_logout_json.restype = c_int

lib.fs_list_json.argtypes = [c_int, c_char_p, c_size_t]
lib.fs_list_json.restype = c_int
lib.fs_mkdir_json.argtypes = [c_int, c_char_p, c_char_p, c_size_t]
lib.fs_mkdir_json.restype = c_int
lib.fs_delete_folder_json.argtypes = [c_int, c_char_p, c_size_t]
lib.fs_delete_folder_json.restype = c_int
lib.fs_delete_file_json.argtypes = [c_int, c_char_p, c_size_t]
lib.fs_delete_file_json.restype = c_int
lib.fs_copy_file_json.argtypes = [c_int, c_int, c_char_p, c_size_t]
lib.fs_copy_file_json.restype = c_int
lib.fs_list_shared_items_json.argtypes = [c_char_p, c_size_t]
lib.fs_list_shared_items_json.restype = c_int
lib.fs_share_folder_json.argtypes = [c_int, c_char_p, c_int, c_char_p, c_size_t]
lib.fs_share_folder_json.restype = c_int
lib.fs_share_file_json.argtypes = [c_int, c_char_p, c_int, c_char_p, c_size_t]
lib.fs_share_file_json.restype = c_int
lib.fs_rename_folder_json.argtypes = [c_int, c_char_p, c_char_p, c_size_t]
lib.fs_rename_folder_json.restype = c_int
lib.fs_rename_file_json.argtypes = [c_int, c_char_p, c_char_p, c_size_t]
lib.fs_rename_file_json.restype = c_int
lib.fs_copy_folder_json.argtypes = [c_int, c_int, c_char_p, c_size_t]
lib.fs_copy_folder_json.restype = c_int
lib.fs_list_folder_permissions_json.argtypes = [c_int, c_char_p, c_size_t]
lib.fs_list_folder_permissions_json.restype = c_int
lib.fs_list_file_permissions_json.argtypes = [c_int, c_char_p, c_size_t]
lib.fs_list_file_permissions_json.restype = c_int
lib.fs_update_folder_permission_json.argtypes = [c_int, c_char_p, c_int, c_char_p, c_size_t]
lib.fs_update_folder_permission_json.restype = c_int
lib.fs_update_file_permission_json.argtypes = [c_int, c_char_p, c_int, c_char_p, c_size_t]
lib.fs_update_file_permission_json.restype = c_int
lib.fs_upload_file_json.argtypes = [c_char_p, c_int, c_char_p, c_size_t]
lib.fs_upload_file_json.restype = c_int
lib.fs_download_file_json.argtypes = [c_char_p, c_int, c_char_p, c_size_t]
lib.fs_download_file_json.restype = c_int
lib.fs_download_folder_json.argtypes = [c_char_p, c_int, c_char_p, c_size_t]
lib.fs_download_folder_json.restype = c_int
lib.fs_cancel_download_json.argtypes = [c_char_p, c_char_p, c_size_t]
lib.fs_cancel_download_json.restype = c_int
lib.fs_cancel_upload_json.argtypes = [c_char_p, c_char_p, c_size_t]
lib.fs_cancel_upload_json.restype = c_int
lib.fs_resume_download_json.argtypes = [c_char_p, c_size_t]
lib.fs_resume_download_json.restype = c_int
lib.fs_resume_upload_json.argtypes = [c_char_p, c_size_t]
lib.fs_resume_upload_json.restype = c_int
lib.fs_search_folders_json.argtypes = [c_char_p, c_char_p, c_size_t]
lib.fs_search_folders_json.restype = c_int
lib.fs_search_files_json.argtypes = [c_char_p, c_char_p, c_size_t]
lib.fs_search_files_json.restype = c_int

_connected = False

def ensure_connected(host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    global _connected
    if _connected:
        return True
    rc = lib.fs_connect(host.encode(), c_uint16(port), timeout)
    _connected = (rc == 0)
    return _connected

def reconnect(host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    lib.fs_disconnect()
    rc = lib.fs_connect(host.encode(), c_uint16(port), timeout)
    _connected = (rc == 0)
    return _connected

# result code:
# 0: OK
# -1: error
# -2: server not response
class RequestResult(Enum):
    OK = 0
    ERROR = -1
    NOT_RESPONSE = -2

# Hàm trả về kết quả, trạng thái của request, và payload json đã được decode
def _call_json(func, *args, buf_size=4096) -> Tuple[RequestResult, str]:
    buf = create_string_buffer(buf_size)
    rc = func(*args, buf, len(buf))
    rr = RequestResult(rc)
    return rr, buf.value.decode(errors="ignore")

def login(username, password, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    if not ensure_connected(host, port, timeout):
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_login_json, username.encode(), password.encode())

def register(username, password, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    if not ensure_connected(host, port, timeout):
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_register_json, username.encode(), password.encode())

def auth(token, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    if not ensure_connected(host, port, timeout):
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_auth_json, token.encode())

def logout():
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_logout_json)

def list_folder(folder_id=1):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_list_json, c_int(folder_id))

def mkdir(parent_id, name):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_mkdir_json, c_int(parent_id), name.encode())

def delete_folder(folder_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_delete_folder_json, c_int(folder_id))

def delete_file(file_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_delete_file_json, c_int(file_id))

def list_shared_items():
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_list_shared_items_json)

def share_folder(folder_id, username, permission):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_share_folder_json, c_int(folder_id), username.encode(), c_int(permission))

def share_file(file_id, username, permission):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_share_file_json, c_int(file_id), username.encode(), c_int(permission))

def rename_folder(folder_id, new_name):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_rename_folder_json, c_int(folder_id), new_name.encode())

def rename_file(file_id, new_name):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_rename_file_json, c_int(file_id), new_name.encode())

def copy_file(file_id, dest_folder_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_copy_file_json, c_int(file_id), c_int(dest_folder_id))

def list_folder_permissions(folder_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_list_folder_permissions_json, c_int(folder_id))

def list_file_permissions(file_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_list_file_permissions_json, c_int(file_id))

def update_folder_permission(folder_id, username, permission):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_update_folder_permission_json, c_int(folder_id), username.encode(), c_int(permission))

def update_file_permission(file_id, username, permission):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_update_file_permission_json, c_int(file_id), username.encode(), c_int(permission))

def copy_folder(folder_id, dest_folder_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_copy_folder_json, c_int(folder_id), c_int(dest_folder_id))

def upload_file(file_path, parent_folder_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_upload_file_json, file_path.encode(), c_int(parent_folder_id))

def download_file(dest_dir, file_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_download_file_json, dest_dir.encode(), c_int(file_id))

def download_folder(dest_dir, folder_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_download_folder_json, dest_dir.encode(), c_int(folder_id))

def cancel_download(session_id, dest_buf_size=4096):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_cancel_download_json, session_id.encode())

def cancel_upload(session_id):
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_cancel_upload_json, session_id.encode())

def resume_download():
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_resume_download_json)  # -1 indicates resume with session_id

def resume_upload():
    if not ensure_connected():
        return RequestResult.ERROR, ""
    return _call_json(lib.fs_resume_upload_json)

def search(keyword: str):
    if not keyword:
        return RequestResult.ERROR, ""
    if not ensure_connected():
        return RequestResult.ERROR, ""

    # call folders
    buf = create_string_buffer(4096)
    rc_f = lib.fs_search_folders_json(keyword.encode(), buf, len(buf))
    folders_raw = buf.value.decode(errors="ignore") if rc_f == 0 else ""

    buf2 = create_string_buffer(4096)
    rc_file = lib.fs_search_files_json(keyword.encode(), buf2, len(buf2))
    files_raw = buf2.value.decode(errors="ignore") if rc_file == 0 else ""

    try:
        items = []
        if folders_raw:
            payload = json.loads(folders_raw)
            if isinstance(payload, dict):
                items.extend(payload.get("items", []))
        if files_raw:
            payload = json.loads(files_raw)
            if isinstance(payload, dict):
                items.extend(payload.get("items", []))
        merged = {"status": "ok", "items": items}
        # Propagate most severe rc
        rc = rc_f if rc_f != 0 else rc_file
        result = RequestResult(rc) if rc in (-2, -1, 0) else RequestResult.ERROR
        return result, json.dumps(merged)
    except json.JSONDecodeError:
        return RequestResult.ERROR, ""
