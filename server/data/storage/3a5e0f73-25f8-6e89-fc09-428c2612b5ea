import json
import os

SESSION_PATH = os.path.join(os.path.dirname(__file__), ".session.json")


def load_session():
    if not os.path.exists(SESSION_PATH):
        return {}
    try:
        with open(SESSION_PATH, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def save_session(username: str, token: str, root_folder_id: int):
    data = {"username": username or "", "token": token or "", "root_folder_id": root_folder_id or 0}
    try:
        with open(SESSION_PATH, "w", encoding="utf-8") as f:
            json.dump(data, f)
    except Exception:
        pass


def clear_session():
    try:
        if os.path.exists(SESSION_PATH):
            os.remove(SESSION_PATH)
    except Exception:
        pass
