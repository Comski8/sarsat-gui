import subprocess
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QPushButton, QVBoxLayout, QHBoxLayout, QFileDialog,
    QTextEdit, QLabel, QComboBox, QWidget
)
from PyQt5.QtCore import pyqtSignal, QObject


class WorkerSignals(QObject):
    append_text = pyqtSignal(str)


class DecoderGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SARSAT Decoder GUI")
        self.setGeometry(100, 100, 800, 600)

        self.signals = WorkerSignals()
        self.signals.append_text.connect(self.update_output)

        self.init_ui()

    def init_ui(self):
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        # Mode selection
        self.mode_label = QLabel("Select Decoding Mode:")
        self.layout.addWidget(self.mode_label)
        self.mode_selector = QComboBox()
        self.mode_selector.addItems([
            "Decode via Microphone (Live Audio)",
            "Decode via SDR",
            "Decode WAV File"
        ])
        self.layout.addWidget(self.mode_selector)

        # File selection for WAV mode
        self.file_button = QPushButton("Select WAV File")
        self.file_button.clicked.connect(self.select_file)
        self.file_button.setEnabled(False)
        self.layout.addWidget(self.file_button)
        self.selected_file = None

        # Output area
        self.output_area = QTextEdit()
        self.output_area.setReadOnly(True)
        self.layout.addWidget(self.output_area)

        # Run, Clear, and Quit buttons
        button_layout = QHBoxLayout()
        self.run_button = QPushButton("Run Decoder")
        self.run_button.clicked.connect(self.run_decoder)
        button_layout.addWidget(self.run_button)

        self.clear_button = QPushButton("Clear Output")
        self.clear_button.clicked.connect(self.clear_output)
        button_layout.addWidget(self.clear_button)

        self.quit_button = QPushButton("Quit")
        self.quit_button.clicked.connect(self.quit_application)
        button_layout.addWidget(self.quit_button)

        self.layout.addLayout(button_layout)

        # Enable/disable file selection based on mode
        self.mode_selector.currentIndexChanged.connect(self.toggle_file_button)

    def toggle_file_button(self):
        if self.mode_selector.currentText() == "Decode WAV File":
            self.file_button.setEnabled(True)
        else:
            self.file_button.setEnabled(False)

    def select_file(self):
        # File selection dialog
        file_path, _ = QFileDialog.getOpenFileName(self, "Select WAV File", "", "WAV Files (*.wav);;All Files (*)")
        if file_path:
            self.selected_file = file_path
            self.signals.append_text.emit(f"Selected WAV file: {file_path}\n")

    def run_decoder(self):
        mode = self.mode_selector.currentText()
        if mode == "Decode via Microphone (Live Audio)":
            command = "./decode_MIC_email_406.pl"
            title = "DECODEUR LIVE AUDIO - FERMER POUR ARRETER"
            self.run_in_terminal(command, title)
        elif mode == "Decode via SDR":
            command = "./scan406.pl 406M 406.1M"
            title = "DECODEUR SDR - FERMER POUR ARRETER"
            self.run_in_terminal(command, title)
        elif mode == "Decode WAV File":
            if not self.selected_file:
                self.signals.append_text.emit("Error: No WAV file selected!\n")
                return
            command = f'./dec406_V7 "{self.selected_file}"'
            self.run_directly(command)
        else:
            self.signals.append_text.emit("Error: Invalid mode selected.\n")

    def run_in_terminal(self, command, title):
        try:
            self.signals.append_text.emit(f"Running command in terminal: {command}\n")
            subprocess.Popen(
                ["gnome-terminal", "--title", title, "--", "bash", "-c", f"{command}; exec bash"],
                shell=False
            )
        except Exception as e:
            self.signals.append_text.emit(f"Error running command in terminal: {e}\n")

    def run_directly(self, command):
        try:
            self.signals.append_text.emit(f"Running command: {command}\n")
            process = subprocess.Popen(
                command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )
            stdout, stderr = process.communicate()
            if stdout:
                self.signals.append_text.emit(stdout)
            if stderr:
                self.signals.append_text.emit(f"Error: {stderr}")
        except Exception as e:
            self.signals.append_text.emit(f"Error running command: {e}\n")

    def update_output(self, text):
        self.output_area.append(text)
        self.output_area.ensureCursorVisible()

    def clear_output(self):
        self.output_area.clear()

    def quit_application(self):
        self.close()


if __name__ == "__main__":
    app = QApplication([])
    window = DecoderGUI()
    window.show()
    app.exec_()
