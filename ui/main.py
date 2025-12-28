import sys
from PyQt5.QtWidgets import QApplication
from Controller.app_controller import AppController


if __name__ == "__main__":
    app = QApplication(sys.argv)

    app_controller = AppController()
    app_controller.start()

    sys.exit(app.exec_())
