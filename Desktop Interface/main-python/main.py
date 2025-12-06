import threading
import re
import sys
import tkinter  # needed for some type hints in customtkinter
import customtkinter as ctk

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

# ---------------------- CONFIGURABLE CONSTANTS ---------------------- #

BAUDRATE = 115200
SERIAL_TIMEOUT = 1.0  # seconds
SCAN_COMMAND = "SCAN"  # what we send to request a scan

# Matches AA:BB:CC:DD:EE:FF style MAC addresses
MAC_REGEX = re.compile(r"([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})")


# --------------------------- SERIAL MANAGER ------------------------- #

class SerialManager:
    """
    Handles serial port connection and background reading.

    - call open_port(port) to connect
    - call close_port() to disconnect
    - call write_line(text) to send a line (adds newline automatically)
    - on_line_callback(line: str) is called whenever a new line is read
      (from a background thread)
    """

    def __init__(self, on_line_callback=None, baudrate=BAUDRATE, timeout=SERIAL_TIMEOUT):
        self.on_line_callback = on_line_callback
        self.baudrate = baudrate
        self.timeout = timeout

        self._serial = None
        self._reader_thread = None
        self._stop_event = threading.Event()

    @property
    def is_open(self) -> bool:
        return self._serial is not None and self._serial.is_open

    @staticmethod
    def list_serial_ports():
        """Return a list of port device names, e.g. ['COM3', '/dev/ttyUSB0']"""
        if list_ports is None:
            return []
        return [p.device for p in list_ports.comports()]

    def open_port(self, port: str):
        """Open the given serial port and start the reader thread."""
        if serial is None:
            raise RuntimeError("pyserial is not installed. Run 'pip install pyserial'.")

        self.close_port()

        self._serial = serial.Serial(
            port=port,
            baudrate=self.baudrate,
            timeout=self.timeout
        )

        self._stop_event.clear()
        self._reader_thread = threading.Thread(
            target=self._reader_loop, daemon=True
        )
        self._reader_thread.start()

    def close_port(self):
        """Close the serial port and stop the reader thread."""
        self._stop_event.set()

        if self._reader_thread is not None:
            self._reader_thread.join(timeout=1.0)
            self._reader_thread = None

        if self._serial is not None:
            if self._serial.is_open:
                self._serial.close()
            self._serial = None

    def write_line(self, text: str):
        """Send a line of text over serial (with newline)."""
        if not self.is_open:
            return
        data = (text + "\n").encode("utf-8", errors="ignore")
        self._serial.write(data)
        self._serial.flush()

    def _reader_loop(self):
        """Background thread: reads lines and calls the callback."""
        while not self._stop_event.is_set():
            try:
                if not self.is_open:
                    break
                line_bytes = self._serial.readline()
                if not line_bytes:
                    continue
                line = line_bytes.decode("utf-8", errors="ignore").strip()
                if line and self.on_line_callback:
                    self.on_line_callback(line)
            except Exception:
                # In production you might log this:
                # print("Error in serial reader:", e)
                break


# ---------------------------- DEVICE CARD --------------------------- #

class DeviceCard(ctk.CTkFrame):
    """
    Represents a single microcontroller/node on the right side.
    You can easily expand this with more labels, buttons, graphs, etc.
    """

    def __init__(self, parent, mac_address: str, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)

        self.mac_address = mac_address

        self.grid_columnconfigure(0, weight=1)

        self.label_title = ctk.CTkLabel(
            self,
            text=f"Device: {mac_address}",
            font=ctk.CTkFont(size=14, weight="bold")
        )
        self.label_title.grid(row=0, column=0, sticky="w", padx=10, pady=(8, 2))

        # Placeholder for more info (RSSI, status, etc.)
        self.label_status = ctk.CTkLabel(
            self,
            text="Status: discovered",
            font=ctk.CTkFont(size=12)
        )
        self.label_status.grid(row=1, column=0, sticky="w", padx=10, pady=(0, 8))

        # Example button; you can wire this up to send commands to this node
        self.button_action = ctk.CTkButton(
            self,
            text="Placeholder Action",
            command=self.on_action_click
        )
        self.button_action.grid(row=0, column=1, rowspan=2,
                                sticky="e", padx=10, pady=8)

    def on_action_click(self):
        # For now, just print. You can hook up something real here.
        print(f"[DeviceCard] Action clicked for {self.mac_address}")


# ------------------------------- APP -------------------------------- #

class MeshApp(ctk.CTk):
    """
    Main application window.

    Layout:
    - Left sidebar (serial controls)
    - Right area with scrollable device cards
    """

    def __init__(self):
        super().__init__()

        # --- Basic window setup ---
        ctk.set_appearance_mode("Dark")      # or "Light", "System"
        ctk.set_default_color_theme("blue")  # can be "green", "dark-blue", etc.

        self.title("Mesh Network Device Scanner")
        self.geometry("1000x600")
        self.minsize(800, 500)

        # serial manager
        self.serial_manager = SerialManager(
            on_line_callback=self._on_serial_line_from_thread
        )

        # used by OptionMenu
        self.selected_port = ctk.StringVar(value="")

        # store device cards: {mac: DeviceCard}
        self.device_cards = {}

        # configure main grid
        self.grid_columnconfigure(0, weight=0)  # sidebar
        self.grid_columnconfigure(1, weight=1)  # main content
        self.grid_rowconfigure(0, weight=1)

        self._build_sidebar()
        self._build_main_area()

        # populate ports on start
        self.refresh_ports()

    # ------------------------ UI BUILDERS ------------------------ #

    def _build_sidebar(self):
        self.sidebar = ctk.CTkFrame(self, width=240)
        self.sidebar.grid(row=0, column=0, sticky="nsw", padx=10, pady=10)
        self.sidebar.grid_propagate(False)

        self.sidebar.grid_rowconfigure(99, weight=1)

        # Title
        label_title = ctk.CTkLabel(
            self.sidebar, text="Serial Connection",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        label_title.grid(row=0, column=0, padx=10, pady=(10, 10), sticky="w")

        # Port selector section
        frame_ports = ctk.CTkFrame(self.sidebar)
        frame_ports.grid(row=1, column=0, padx=10, pady=(0, 15), sticky="ew")
        frame_ports.grid_columnconfigure(0, weight=1)

        label_port = ctk.CTkLabel(frame_ports, text="Port:", anchor="w")
        label_port.grid(row=0, column=0, sticky="w", pady=(10, 5), padx=10)

        self.optionmenu_ports = ctk.CTkOptionMenu(
            frame_ports,
            values=[],
            variable=self.selected_port
        )
        self.optionmenu_ports.grid(row=1, column=0, sticky="ew", padx=10, pady=(0, 10))

        button_refresh = ctk.CTkButton(
            frame_ports,
            text="Refresh Ports",
            command=self.refresh_ports
        )
        button_refresh.grid(row=2, column=0, sticky="ew", padx=10, pady=(0, 10))

        # Connect / Disconnect section
        frame_connect = ctk.CTkFrame(self.sidebar)
        frame_connect.grid(row=2, column=0, padx=10, pady=(0, 10), sticky="ew")
        frame_connect.grid_columnconfigure(0, weight=1)

        self.button_connect = ctk.CTkButton(
            frame_connect,
            text="Connect & Scan",
            command=self.on_connect_clicked
        )
        self.button_connect.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 5))

        self.label_status = ctk.CTkLabel(
            frame_connect,
            text="Status: Disconnected",
            wraplength=200,
            anchor="w",
            justify="left"
        )
        self.label_status.grid(row=1, column=0, sticky="w", padx=10, pady=(0, 10))

    def _build_main_area(self):
        self.main_area = ctk.CTkFrame(self)
        self.main_area.grid(row=0, column=1, sticky="nsew", padx=15, pady=15)

        self.main_area.grid_rowconfigure(1, weight=1)
        self.main_area.grid_columnconfigure(0, weight=1)

        label_devices = ctk.CTkLabel(
            self.main_area,
            text="Discovered Devices",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        label_devices.grid(row=0, column=0, sticky="w", pady=(0, 10), padx=5)

        self.devices_frame = ctk.CTkScrollableFrame(
            self.main_area,
            label_text="Mesh Nodes",
        )
        self.devices_frame.grid(row=1, column=0, sticky="nsew", padx=5, pady=5)

        self.devices_frame.grid_columnconfigure(0, weight=1)

    # ---------------------- SERIAL UI HANDLERS ---------------------- #

    def refresh_ports(self):
        ports = self.serial_manager.list_serial_ports()
        if not ports:
            ports = ["<no ports>"]
            self.selected_port.set(ports[0])
        else:
            # If current selection not in new list, reset.
            if self.selected_port.get() not in ports:
                self.selected_port.set(ports[0])

        self.optionmenu_ports.configure(values=ports)

    def on_connect_clicked(self):
        # toggle behavior
        if self.serial_manager.is_open:
            # disconnect
            self.serial_manager.close_port()
            self.button_connect.configure(text="Connect & Scan")
            self._update_status("Status: Disconnected")
        else:
            port = self.selected_port.get().strip()
            if not port or port == "<no ports>":
                self._update_status("Status: No valid serial port selected.")
                return

            try:
                self.serial_manager.open_port(port)
            except Exception as e:
                self._update_status(f"Status: Failed to open {port}: {e}")
                return

            self.button_connect.configure(text="Disconnect")
            self._update_status(f"Status: Connected to {port}. Sending scan request...")

            # When we first connect, immediately send a scan
            self.send_scan_request()

    def send_scan_request(self):
        if not self.serial_manager.is_open:
            self._update_status("Status: Not connected.")
            return

        # Optionally clear devices each scan:
        # self.clear_devices()
        self.serial_manager.write_line(SCAN_COMMAND)

    def _update_status(self, text: str):
        self.label_status.configure(text=text)
        print(text)

    # --------------------- SERIAL LINE PROCESSING -------------------- #

    def _on_serial_line_from_thread(self, line: str):
        """
        Called by SerialManager's background thread.

        We must NOT touch tkinter widgets from that thread,
        so we schedule a call onto the main thread using `after`.
        """
        self.after(0, self.process_serial_line, line)

    def process_serial_line(self, line: str):
        """
        Main-thread-safe processing of incoming serial data.

        This is where we extract MAC addresses and create cards.
        """

        print(f"[Serial] {line}")

        matches = MAC_REGEX.findall(line)
        if not matches:
            return

        for mac in matches:
            normalized = mac.upper()
            self.add_or_get_device_card(normalized)

    # ----------------------- DEVICE CARD MGMT ------------------------ #

    def add_or_get_device_card(self, mac_address: str) -> DeviceCard:
        """
        Get an existing card for `mac_address`, or create a new one.

        This makes it easy for future code to retrieve and update
        a device card (e.g., to update status or show sensor values).
        """
        if mac_address in self.device_cards:
            return self.device_cards[mac_address]

        card = DeviceCard(
            self.devices_frame,
            mac_address=mac_address,
            corner_radius=10
        )

        # Use pack for simple vertical stacking.
        card.pack(fill="x", padx=10, pady=5)

        self.device_cards[mac_address] = card
        return card

    def clear_devices(self):
        """Remove all device cards."""
        for card in self.device_cards.values():
            card.destroy()
        self.device_cards.clear()

    # ------------------------------ MISC ------------------------------ #

    def on_closing(self):
        """Cleanup on window close."""
        self.serial_manager.close_port()
        self.destroy()


# ----------------------------- MAIN --------------------------------- #

def main():
    app = MeshApp()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()


if __name__ == "__main__":
    main()
