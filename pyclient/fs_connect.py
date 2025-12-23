import ctypes
import os
import threading
from ctypes import (
    c_char_p,
    c_int,
    c_uint16,
    c_uint32,
    c_uint64,
    c_void_p,
    POINTER,
)

_here = os.path.dirname(os.path.abspath(__file__))
_lib_path = os.path.join(_here, "..", "client", "libclient.so")
_lib = ctypes.CDLL(_lib_path)


class FsClient(ctypes.Structure):
    """Opaque handle for FsClient in C."""

    pass


FSClientPtr = POINTER(FsClient)

# FsApiCallback in C: void (*)(int status, const char *json_resp, void *user_data)
FsApiCallbackC = ctypes.CFUNCTYPE(None, c_int, c_char_p, c_void_p)


# --- Bind low-level creation/destruction ---
_lib.fs_client_create.argtypes = [c_char_p, c_uint16]
_lib.fs_client_create.restype = FSClientPtr

_lib.fs_client_destroy.argtypes = [FSClientPtr]
_lib.fs_client_destroy.restype = None

# --- Bind high-level JSON async APIs ---
_lib.fs_api_register.argtypes = [
    FSClientPtr,
    c_char_p,
    c_char_p,
    FsApiCallbackC,
    c_void_p,
]
_lib.fs_api_register.restype = c_int

_lib.fs_api_login.argtypes = [
    FSClientPtr,
    c_char_p,
    c_char_p,
    FsApiCallbackC,
    c_void_p,
]
_lib.fs_api_login.restype = c_int

_lib.fs_api_list.argtypes = [
    FSClientPtr,
    c_int,
    FsApiCallbackC,
    c_void_p,
]
_lib.fs_api_list.restype = c_int

_lib.fs_api_mkdir.argtypes = [
    FSClientPtr,
    c_int,
    c_char_p,
    FsApiCallbackC,
    c_void_p,
]
_lib.fs_api_mkdir.restype = c_int

_lib.fs_api_upload_init.argtypes = [
    FSClientPtr,
    c_char_p,
    c_uint64,
    c_uint32,
    FsApiCallbackC,
    c_void_p,
]
_lib.fs_api_upload_init.restype = c_int

_lib.fs_api_download.argtypes = [
    FSClientPtr,
    c_char_p,
    FsApiCallbackC,
    c_void_p,
]
_lib.fs_api_download.restype = c_int


class Client:
    """
    Python wrapper around libclient.so high-level async API.

    Exposes synchronous-style methods (register/login/list_folder/...) by
    internally waiting for the C async callback on a worker thread.
    """

    def __init__(self, host: str = "127.0.0.1", port: int = 5555) -> None:
        self._fc: FSClientPtr = _lib.fs_client_create( # type: ignore
            host.encode("utf-8"), c_uint16(port)
        )
        if not self._fc:
            raise RuntimeError("Failed to create FsClient / connect to server")

        # Keep references to callbacks to avoid them being GC'd
        self._callbacks: list[FsApiCallbackC] = [] # type: ignore

    def close(self) -> None:
        if self._fc:
            _lib.fs_client_destroy(self._fc)
            self._fc = None
        self._callbacks.clear()

    def __enter__(self) -> "Client":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    # ---- internal helper ----
    def _call_and_wait(self, c_func, *c_args) -> tuple[int, str | None]:
        """
        Call a C async API and block current Python thread until callback fires.
        Returns (status, json_string_or_None).
        """
        if not self._fc:
            raise RuntimeError("Client is closed")

        done = threading.Event()
        result: dict[str, object] = {"status": -1, "json": None}

        def _py_cb(status: int, json_c: bytes | None, user_data: object) -> None:
            try:
                result["status"] = int(status)
                result["json"] = (
                    json_c.decode("utf-8") if json_c is not None else None
                )
            finally:
                done.set()

        cb = FsApiCallbackC(_py_cb)
        self._callbacks.append(cb)

        rc = c_func(self._fc, *c_args, cb, None)
        if rc != 0:
            # Call failed before sending; drop callback and return error
            try:
                self._callbacks.remove(cb)
            except ValueError:
                pass
            return rc, None

        done.wait()

        try:
            self._callbacks.remove(cb)
        except ValueError:
            pass

        status = int(result["status"])
        json_resp = result["json"]
        return status, json_resp if isinstance(json_resp, str) else None

    # ---- public synchronous-style methods ----
    def register(self, username: str, password: str) -> tuple[int, str | None]:
        return self._call_and_wait(
            _lib.fs_api_register,
            username.encode("utf-8"),
            password.encode("utf-8"),
        )

    def login(self, username: str, password: str) -> tuple[int, str | None]:
        return self._call_and_wait(
            _lib.fs_api_login,
            username.encode("utf-8"),
            password.encode("utf-8"),
        )

    def list_folder(self, folder_id: int) -> tuple[int, str | None]:
        return self._call_and_wait(
            _lib.fs_api_list,
            c_int(folder_id),
        )

    def mkdir(self, parent_id: int, name: str) -> tuple[int, str | None]:
        return self._call_and_wait(
            _lib.fs_api_mkdir,
            c_int(parent_id),
            name.encode("utf-8"),
        )

    def upload_init(
        self,
        path: str,
        file_size: int,
        chunk_size: int,
    ) -> tuple[int, str | None]:
        return self._call_and_wait(
            _lib.fs_api_upload_init,
            path.encode("utf-8"),
            c_uint64(file_size),
            c_uint32(chunk_size),
        )

    def download(self, path: str) -> tuple[int, str | None]:
        return self._call_and_wait(
            _lib.fs_api_download,
            path.encode("utf-8"),
        )

