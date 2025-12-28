from PyQt5.QtWidgets import QStyle


class MainBinder:
    def __init__(self, style):
        self.style = style

    def load_data(self):
        icon_folder = self.style.standardIcon(QStyle.SP_DirIcon)
        icon_file = self.style.standardIcon(QStyle.SP_FileIcon)

        return [
            {
                "name": "Folder A",
                "owner": "manhnd52",
                "time": "Hôm nay, 15:23",
                "size": "-",
                "is_folder": True,
                "icon": icon_folder,
            },
            {
                "name": "Folder B",
                "owner": "manhnd52",
                "time": "29 th9",
                "size": "-",
                "is_folder": True,
                "icon": icon_folder,
            },
            {
                "name": "File A",
                "owner": "manhnd52",
                "time": "Hôm nay, 15:23",
                "size": "2 MB",
                "is_folder": False,
                "icon": icon_file,
            },
            {
                "name": "File B",
                "owner": "manhnd52",
                "time": "Hôm nay, 15:23",
                "size": "3 MB",
                "is_folder": False,
                "icon": icon_file,
            },
            {
                "name": "File C",
                "owner": "manhnd52",
                "time": "Hôm nay, 15:23",
                "size": "2.3 GB",
                "is_folder": False,
                "icon": icon_file,
            },
        ]
