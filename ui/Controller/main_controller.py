from DataBinder.main_binder import MainBinder
from Controller.share_controller import ShareController


class MainController:
    def __init__(self, view):
        self.view = view
        self.main_binder = MainBinder(view.style())
        self.share_controller = ShareController(view)

        self.view.request_context_menu.connect(self.on_context_menu)
        self.view.request_share.connect(self.on_share)

        self.load()

    def load(self):
        self.view.setEnabled(False)
        try:
            data = self.main_binder.load_data()
            self.view.set_table_data(data)
        finally:
            self.view.setEnabled(True)

    def on_context_menu(self, row):
        self.view.show_context_menu(row)

    def on_share(self, name):
        self.share_controller.open_share(name)
