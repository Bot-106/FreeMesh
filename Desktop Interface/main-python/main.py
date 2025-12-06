import threading
import queue
import time
import serial
import serial.tools.list_ports
import customtkinter as ctk
from typing import Optional


# ------------- Node Card (one per MAC) ------------- #

class NodeCard(ctk.CTkFrame):
    """
    A UI card representing a single microcontroller/node on the mesh.
    """

    def __init__(self, master, mac_address: str, send_callback, *args, **kwargs):
        super().__init__(master, *args, **kwargs)
        self.mac_address = mac_address
        self.send_callback = send_callback  # function(mac: str, text: str)

        self.grid_columnconfigure(1, weight=1)

        self.label_mac = ctk.CTkLabel(self, text=f"MAC: {self.mac_address}")
        self.label_mac.grid(row=0, column=0, columnspan=2, padx=10, pady=(10, 5), sticky="w")

        self.entry_text = ctk.CTkEntry(self, placeholder_text="Enter text to send...")
        self.entry_text.grid(row=1, column=0, padx=10, pady=5, sticky="we")

        self.button_send = ctk.CTkButton(self, text="Send Text", command=self.on_send_clicked)
        self.button_send.grid(row=1, column=1, padx=10, pady=5)

    def on_send_clicked(self):
        text = self.entry_text.get().strip()
        if text:
            self.send_callback(self.mac_address, text)


# ------------- Serial Worker Thread ------------- #

class SerialWorker(threading.Thread):
    """
    Background thread that reads *lines* from a serial port and pushes them into a queue.
    Uses readline() with a timeout so each line is processed cleanly.
    """

    def __init__(self, ser: serial.Serial, line_queue: queue.Queue, stop_event: threading.Event):
        super().__init__(daemon=True)
        self.ser = ser
        self.line_queue = line_queue
        self.stop_event = stop_event

    def run(self):
        while not self.stop_event.is_set():
            try:
                line_bytes = self.ser.readline()
                if line_bytes:
                    try:
                        text = line_bytes.decode("utf-8", errors="ignore").strip()
                    except Exception:
                        text = ""
                    if text:
                        self.line_queue.put(text)
                else:
                    time.sleep(0.01)

            except serial.SerialException:
                break
            except Exception:
                time.sleep(0.05)


# ------------- Main Application ------------- #

class MeshApp(ctk.CTk):

    def __init__(self):
        super().__init__()

        self.title("Mesh Network Serial UI")
        self.geometry("900x600")

        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        # Serial-related state
        self.serial_port = None              # type: Optional[serial.Serial]
        self.serial_thread = None            # type: Optional[SerialWorker]
        self.serial_stop_event = threading.Event()
        self.serial_line_queue = queue.Queue()

        # Node cards by MAC string
        self.node_cards = {}  # mac -> NodeCard

        # Layout
        self.grid_columnconfigure(0, weight=0)
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        self._create_sidebar()
        self._create_main_area()

        # Start polling queue
        self.after(50, self._poll_serial_queue)

    # ----- UI creation -----

    def _create_sidebar(self):
        self.sidebar = ctk.CTkFrame(self, width=200, corner_radius=0)
        self.sidebar.grid(row=0, column=0, sticky="nswe")
        self.sidebar.grid_rowconfigure(10, weight=1)

        label_title = ctk.CTkLabel(
            self.sidebar,
            text="Mesh Controller",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        label_title.grid(row=0, column=0, padx=10, pady=(10, 5), sticky="w")

        label_port = ctk.CTkLabel(self.sidebar, text="Serial Port:")
        label_port.grid(row=1, column=0, padx=10, pady=(10, 0), sticky="w")

        self.port_var = ctk.StringVar()
        self.combo_ports = ctk.CTkComboBox(self.sidebar, variable=self.port_var, values=[])
        self.combo_ports.grid(row=2, column=0, padx=10, pady=5, sticky="we")

        self.button_refresh_ports = ctk.CTkButton(
            self.sidebar, text="Refresh Ports", command=self.refresh_ports
        )
        self.button_refresh_ports.grid(row=3, column=0, padx=10, pady=5, sticky="we")

        self.button_connect = ctk.CTkButton(
            self.sidebar, text="Connect", command=self.on_connect_clicked
        )
        self.button_connect.grid(row=4, column=0, padx=10, pady=10, sticky="we")

        self.button_scan = ctk.CTkButton(
            self.sidebar, text="Scan All", command=self.on_scan_all_clicked, state="disabled"
        )
        self.button_scan.grid(row=5, column=0, padx=10, pady=5, sticky="we")

        self.label_status = ctk.CTkLabel(self.sidebar, text="Disconnected", text_color="red")
        self.label_status.grid(row=6, column=0, padx=10, pady=10, sticky="w")

        self.refresh_ports()

    def _create_main_area(self):
        self.main_frame = ctk.CTkFrame(self)
        self.main_frame.grid(row=0, column=1, sticky="nswe")
        self.main_frame.grid_rowconfigure(0, weight=1)
        self.main_frame.grid_columnconfigure(0, weight=1)

        self.card_container = ctk.CTkScrollableFrame(self.main_frame)
        self.card_container.grid(row=0, column=0, padx=10, pady=10, sticky="nswe")

    # ----- Serial Port Management -----

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        values = [p.device for p in ports]
        self.combo_ports.configure(values=values)
        if values:
            self.port_var.set(values[0])
        else:
            self.port_var.set("")

    def on_connect_clicked(self):
        if self.serial_port and self.serial_port.is_open:
            return

        port_name = self.port_var.get().strip()
        if not port_name:
            self._set_status("No port selected", error=True)
            return

        # Cleanup older connection
        if self.serial_port:
            try:
                self.serial_stop_event.set()
                time.sleep(0.1)
                if self.serial_port.is_open:
                    self.serial_port.close()
            except Exception:
                pass
        self.serial_port = None
        self.serial_stop_event = threading.Event()

        try:
            self.serial_port = serial.Serial(
                port=port_name,
                baudrate=115200,
                timeout=0.1,
                write_timeout=0.5
            )
        except serial.SerialException as e:
            self._set_status(f"Failed to open {port_name}", error=True)
            print("Serial Exception:", e)
            return

        # Flush buffers
        try:
            self.serial_port.reset_input_buffer()
            self.serial_port.reset_output_buffer()
        except Exception:
            pass

        # Launch worker thread
        self.serial_thread = SerialWorker(self.serial_port, self.serial_line_queue, self.serial_stop_event)
        self.serial_thread.start()

        self._set_status(f"Connected to {port_name}", error=False)
        self.button_scan.configure(state="normal")
        self.button_connect.configure(state="disabled")
        self.combo_ports.configure(state="disabled")
        self.button_refresh_ports.configure(state="disabled")

        # Auto-scan
        self.send_line("--scan-all")

    def _set_status(self, text, error=False):
        color = "red" if error else "green"
        self.label_status.configure(text=text, text_color=color)

    def on_scan_all_clicked(self):
        self.send_line("--scan-all")

    def send_line(self, line: str):
        if not self.serial_port or not self.serial_port.is_open:
            self._set_status("Not connected", error=True)
            return
        try:
            full = (line + "\n").encode("utf-8")
            self.serial_port.write(full)
            self.serial_port.flush()
            print("[SERIAL OUT]", line)
        except Exception as e:
            print("Serial write error:", e)
            self._set_status("Serial write error", error=True)

    # ----- Serial Listener -----

    def _poll_serial_queue(self):
        try:
            while True:
                line = self.serial_line_queue.get_nowait()
                self._handle_serial_line(line)
        except queue.Empty:
            pass

        self.after(50, self._poll_serial_queue)

    def _handle_serial_line(self, line: str):
        line = line.strip()
        if not line:
            return

        print("[SERIAL IN]", line)

        if line.startswith("--scan-response"):
            parts = line.split()
            if len(parts) >= 2:
                mac = parts[1]
                self.add_node_card(mac)

    # ----- Card Management -----

    def add_node_card(self, mac: str):
        if mac in self.node_cards:
            return

        card = NodeCard(
            master=self.card_container,
            mac_address=mac,
            send_callback=self.on_node_send
        )
        card.pack(fill="x", padx=5, pady=5)
        self.node_cards[mac] = card

    def on_node_send(self, mac: str, text: str):
        cmd = f"--send-data {mac} {text}"
        self.send_line(cmd)

    # ----- Cleanup -----

    def on_closing(self):
        if self.serial_port:
            try:
                self.serial_stop_event.set()
                time.sleep(0.1)
                if self.serial_port.is_open:
                    self.serial_port.close()
            except Exception:
                pass
        self.destroy()


if __name__ == "__main__":
    app = MeshApp()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
