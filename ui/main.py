import sys
from PyQt5.QtWidgets import QApplication
from Controller.app_controller import AppController
from PyQt5.QtGui import QIcon


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setWindowIcon(QIcon("assets/icon.png"))

    app_controller = AppController()
    app_controller.start()

    sys.exit(app.exec_())
