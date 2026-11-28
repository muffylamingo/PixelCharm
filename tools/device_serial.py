"""Talking to the PixelCharm board over its USB serial port, shared by the scripts in this folder."""
import time

import serial  # pyserial; PlatformIO's own Python already has it

BAUD = 115200


def open_without_reset(port):
    """Opens the port without restarting the board.

    The board has a small auto-reset circuit on the DTR and RTS lines (esptool uses it to start an upload).
    Keeping both lines off while the port opens leaves the running program alone.
    """
    connection = serial.Serial()
    connection.port = port
    connection.baudrate = BAUD
    connection.timeout = 1
    connection.dtr = False
    connection.rts = False
    connection.open()
    time.sleep(0.1)
    connection.reset_input_buffer()
    return connection


def wait_for_line(connection, prefix, seconds):
    """Reads lines until one starts with `prefix` and returns it. Raises TimeoutError if none comes."""
    deadline = time.time() + seconds
    while time.time() < deadline:
        line = connection.readline().decode("utf-8", errors="replace").strip()
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"no '{prefix}' line from the board in {seconds} s")
