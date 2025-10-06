#!/usr/bin/env python3
"""Simple Tkinter GUI to visualise CAN messages streamed as CSV from the Arduino sketch.

The sketch is expected to emit lines in the format:
  timestamp,id,dlc,data0,data1,data2,data3,data4,data5,data6,data7,interpretation

Example:
  12345,0x07F,8,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,MotorCtrl reg 0x90 data 0x01 0x02

Each new CAN identifier gets its own row; subsequent updates refresh the
existing row in-place so the table stays compact.
"""

from __future__ import annotations

import argparse
import csv
import queue
import threading
import time
from dataclasses import dataclass
from typing import Dict, List, Optional

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover - handled at runtime
    raise SystemExit(
        "pyserial is required for this tool. Install it with 'pip install pyserial'."
    ) from exc

import tkinter as tk
from tkinter import ttk


@dataclass
class CANFrame:
    """Parsed representation of one CSV line."""

    timestamp_ms: int
    can_id: str
    dlc: int
    data_bytes: List[str]
    interpretation: str

    @property
    def data_display(self) -> str:
        payload = self.data_bytes[: self.dlc]
        return " ".join(payload)

    @property
    def elapsed_seconds(self) -> str:
        return f"{self.timestamp_ms / 1000.0:.3f}s"


def parse_csv_line(line: str) -> Optional[CANFrame]:
    """Parse a CSV line into a CANFrame, returning None when the format does not match."""

    try:
        row = next(csv.reader([line], skipinitialspace=True))
    except Exception:
        return None

    if not row or row[0].lower() == "timestamp":
        return None

    # At minimum we need timestamp, id, dlc, and eight data columns
    if len(row) < 11:
        return None

    try:
        timestamp_ms = int(row[0])
        can_id = row[1]
        dlc = int(row[2])
    except ValueError:
        return None

    data_bytes = [value.strip() for value in row[3:11]]
    interpretation = row[11].strip() if len(row) > 11 else ""

    return CANFrame(
        timestamp_ms=timestamp_ms,
        can_id=can_id,
        dlc=max(0, min(8, dlc)),
        data_bytes=data_bytes,
        interpretation=interpretation,
    )


def serial_reader(serial_port: serial.Serial, q: queue.Queue[CANFrame], stop_event: threading.Event) -> None:
    """Continuously read lines from the serial port and push parsed frames into a queue."""

    with serial_port:
        serial_port.reset_input_buffer()
        while not stop_event.is_set():
            try:
                line = serial_port.readline().decode("utf-8", errors="ignore").strip()
            except serial.SerialException:
                break

            if not line:
                continue

            frame = parse_csv_line(line)
            if frame is None:
                continue

            q.put(frame)

        # Drain serial buffer if closing
        serial_port.reset_input_buffer()


class CANMonitorGUI:
    def __init__(self, root: tk.Tk, q: queue.Queue[CANFrame], stop_event: threading.Event) -> None:
        self.root = root
        self.queue = q
        self.stop_event = stop_event
        self.rows: Dict[str, str] = {}

        self.root.title("CAN Message Monitor")
        self.root.geometry("720x400")

        self.status_var = tk.StringVar(value="Connecting...")
        status_label = ttk.Label(self.root, textvariable=self.status_var, anchor="w")
        status_label.pack(fill=tk.X, padx=8, pady=(8, 4))

        columns = ("id", "data", "timestamp", "interpretation")
        self.tree = ttk.Treeview(self.root, columns=columns, show="headings")
        self.tree.heading("id", text="CAN ID")
        self.tree.heading("data", text="Hex Data")
        self.tree.heading("timestamp", text="Last Timestamp")
        self.tree.heading("interpretation", text="Interpretation")

        self.tree.column("id", width=90, anchor=tk.CENTER)
        self.tree.column("data", width=250, anchor=tk.W)
        self.tree.column("timestamp", width=120, anchor=tk.CENTER)
        self.tree.column("interpretation", width=220, anchor=tk.W)

        self.tree.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        self.root.after(100, self.process_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def set_status(self, message: str) -> None:
        self.status_var.set(message)

    def process_queue(self) -> None:
        try:
            while True:
                frame = self.queue.get_nowait()
                self.update_row(frame)
        except queue.Empty:
            pass

        if not self.stop_event.is_set():
            self.root.after(100, self.process_queue)

    def update_row(self, frame: CANFrame) -> None:
        values = (
            frame.can_id,
            frame.data_display,
            frame.elapsed_seconds,
            frame.interpretation,
        )

        item_id = self.rows.get(frame.can_id)
        if item_id is None:
            item_id = self.tree.insert("", tk.END, values=values)
            self.rows[frame.can_id] = item_id
        else:
            self.tree.item(item_id, values=values)

    def on_close(self) -> None:
        self.stop_event.set()
        self.root.quit()


def main() -> None:
    parser = argparse.ArgumentParser(description="Visualise CAN messages streamed from an Arduino over serial.")
    parser.add_argument("port", help="Serial port to open, e.g. /dev/tty.usbmodem101")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate to use (default: 115200)")
    parser.add_argument("--timeout", type=float, default=0.5, help="Serial read timeout in seconds (default: 0.5)")
    args = parser.parse_args()

    try:
        serial_port = serial.Serial(args.port, baudrate=args.baud, timeout=args.timeout)
    except serial.SerialException as exc:
        raise SystemExit(f"Failed to open serial port {args.port!r}: {exc}") from exc

    stop_event = threading.Event()
    frame_queue: queue.Queue[CANFrame] = queue.Queue()

    reader_thread = threading.Thread(
        target=serial_reader,
        args=(serial_port, frame_queue, stop_event),
        daemon=True,
        name="SerialReader",
    )
    reader_thread.start()

    root = tk.Tk()
    gui = CANMonitorGUI(root, frame_queue, stop_event)
    gui.set_status(f"Connected to {args.port} @ {args.baud} baud")

    try:
        root.mainloop()
    finally:
        stop_event.set()
        reader_thread.join(timeout=1.0)


if __name__ == "__main__":
    main()
