import json
import os
from ctypes import CDLL, c_char_p, c_uint16, c_int, c_size_t, create_string_buffer

DEFAULT_HOST = os.environ.get("FS_HOST", "192.168.1.182")
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

_connected = False

def ensure_connected(host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    global _connected
    if _connected:
        return True
    rc = lib.fs_connect(host.encode(), c_uint16(port), timeout)
    _connected = (rc == 0)
    return _connected

def _call_json(func, *args, buf_size=4096):
    buf = create_string_buffer(buf_size)
    rc = func(*args, buf, len(buf))
    return (rc == 0), buf.value.decode(errors="ignore")

def login(username, password, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    if not ensure_connected(host, port, timeout):
        return False, ""
    return _call_json(lib.fs_login_json, username.encode(), password.encode())

def register(username, password, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    if not ensure_connected(host, port, timeout):
        return False, ""
    return _call_json(lib.fs_register_json, username.encode(), password.encode())

def auth(token, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
    if not ensure_connected(host, port, timeout):
        return False, ""
    return _call_json(lib.fs_auth_json, token.encode())

def logout():
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_logout_json)

def list_folder(folder_id=1):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_list_json, c_int(folder_id))

def mkdir(parent_id, name):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_mkdir_json, c_int(parent_id), name.encode())

def delete_folder(folder_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_delete_folder_json, c_int(folder_id))

def delete_file(file_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_delete_file_json, c_int(file_id))

def list_shared_items():
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_list_shared_items_json)

def share_folder(folder_id, username, permission):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_share_folder_json, c_int(folder_id), username.encode(), c_int(permission))

def share_file(file_id, username, permission):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_share_file_json, c_int(file_id), username.encode(), c_int(permission))

def rename_folder(folder_id, new_name):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_rename_folder_json, c_int(folder_id), new_name.encode())

def rename_file(file_id, new_name):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_rename_file_json, c_int(file_id), new_name.encode())

def list_folder_permissions(folder_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_list_folder_permissions_json, c_int(folder_id))

def list_file_permissions(file_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_list_file_permissions_json, c_int(file_id))

def update_folder_permission(folder_id, username, permission):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_update_folder_permission_json, c_int(folder_id), username.encode(), c_int(permission))

def update_file_permission(file_id, username, permission):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_update_file_permission_json, c_int(file_id), username.encode(), c_int(permission))

def upload_file(file_path, parent_folder_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_upload_file_json, file_path.encode(), c_int(parent_folder_id))

def download_file(dest_dir, file_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_download_file_json, dest_dir.encode(), c_int(file_id))

def download_folder(dest_dir, folder_id):
    if not ensure_connected():
        return False, ""
    return _call_json(lib.fs_download_folder_json, dest_dir.encode(), c_int(folder_id))
