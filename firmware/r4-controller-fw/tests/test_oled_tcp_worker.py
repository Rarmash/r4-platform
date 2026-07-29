from __future__ import annotations

import queue
import socket
import sys
import threading
import time
import unittest
from pathlib import Path

EMULATOR_DIR = Path(__file__).resolve().parents[1] / "oled-emulator"
sys.path.insert(0, str(EMULATOR_DIR))

from r4_oled_gui import TcpWorker  # noqa: E402
from r4_oled_protocol import framebuffer_hash  # noqa: E402
from r4_tcp_protocol import TcpEndpoint  # noqa: E402


class MockRelay(threading.Thread):
    def __init__(self) -> None:
        super().__init__(daemon=True)
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen()
        self.listener.settimeout(0.2)
        self.port = self.listener.getsockname()[1]
        self.stop_requested = threading.Event()
        self.commands: queue.Queue[str] = queue.Queue()
        self.frame_bytes = bytes(1024)
        self.frame_hash = framebuffer_hash(bytes(128 * 64))

    def stop(self) -> None:
        self.stop_requested.set()
        self.listener.close()

    def response_for(self, command: str) -> str:
        if command == "PING":
            return "PONG"
        if command == "VERSION":
            return "R4_CONTROLLER_FW 0.12.0"
        if command == "HOST HEARTBEAT":
            return "OK HOST HEARTBEAT"
        if command == "INPUT":
            return "INPUT BUTTONS=0000 LX=0 LY=0 RX=0 RY=0"
        if command == "EVENT NEXT":
            return "EVENT NONE"
        if command.startswith("FRAMEBUFFER READ"):
            if command == f"FRAMEBUFFER READ HASH={self.frame_hash:08X}":
                return (
                    f"FRAMEBUFFER UNCHANGED HASH={self.frame_hash:08X}"
                )
            return (
                "FRAMEBUFFER FULL WIDTH=128 HEIGHT=64 "
                f"FORMAT=MONO1_MSB BYTES=1024 HASH={self.frame_hash:08X} "
                f"HEX={self.frame_bytes.hex().upper()}"
            )
        if command.startswith("HOST STATE "):
            return "OK HOST STATE"
        return "ERR UNKNOWN"

    def run(self) -> None:
        while not self.stop_requested.is_set():
            try:
                connection, _ = self.listener.accept()
            except (OSError, TimeoutError):
                continue
            connection.settimeout(0.5)
            with connection, connection.makefile("rwb", buffering=0) as stream:
                while not self.stop_requested.is_set():
                    try:
                        raw = stream.readline(642)
                    except OSError:
                        break
                    if not raw:
                        break
                    line = raw.rstrip(b"\r\n").decode("ascii")
                    _, request_id, command = line.split(" ", 2)
                    self.commands.put(command)
                    response = self.response_for(command)
                    stream.write(
                        f"RES {request_id} OK {response}\n".encode("ascii")
                    )


class TcpWorkerTests(unittest.TestCase):
    def test_receives_real_frame_and_forwards_host_command(self) -> None:
        relay = MockRelay()
        relay.start()
        messages: queue.Queue[tuple[str, object]] = queue.Queue()
        worker = TcpWorker(TcpEndpoint("127.0.0.1", relay.port), messages)
        worker.start()
        worker.submit("HOST STATE MODE=HOME")

        deadline = time.monotonic() + 5.0
        saw_frame = False
        saw_connected = False
        saw_host_command = False
        try:
            while time.monotonic() < deadline:
                try:
                    kind, value = messages.get(timeout=0.1)
                except queue.Empty:
                    continue
                if kind == "frame":
                    saw_frame = value.width == 128 and value.height == 64
                if kind == "status" and str(value).endswith(
                    "Batocera <-> RP2040: connected"
                ):
                    saw_connected = True

                while True:
                    try:
                        command = relay.commands.get_nowait()
                    except queue.Empty:
                        break
                    if command == "HOST STATE MODE=HOME":
                        saw_host_command = True

                if saw_frame and saw_connected and saw_host_command:
                    break
        finally:
            worker.stop()
            relay.stop()
            worker.join(timeout=1.5)
            relay.join(timeout=1.5)

        self.assertTrue(saw_connected)
        self.assertTrue(saw_frame)
        self.assertTrue(saw_host_command)


if __name__ == "__main__":
    unittest.main()
