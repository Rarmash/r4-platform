#!/usr/bin/env python3
"""Interactive Windows bridge for the RP2040-owned R4 OLED framebuffer."""

from __future__ import annotations

import argparse
import queue
import socket
import subprocess
import tempfile
import threading
import time
from pathlib import Path
import tkinter as tk
from tkinter import ttk

from r4_oled_protocol import (
    MAX_PROTOCOL_LINE,
    ProtocolError,
    encode_text,
    fetch_frame,
    fetch_network_frame,
    matches_r4_port,
    read_pbm,
)
from r4_tcp_protocol import (
    BridgeCommandError,
    TcpEndpoint,
    format_request,
    parse_endpoint,
    parse_response,
)


class SerialWorker(threading.Thread):
    def __init__(self, messages: queue.Queue[tuple[str, object]]) -> None:
        super().__init__(daemon=True)
        self.messages = messages
        self.commands: queue.Queue[str] = queue.Queue()
        self.stop_requested = threading.Event()
        self.serial_port = None

    def submit(self, command: str) -> None:
        self.commands.put(command)

    def stop(self) -> None:
        self.stop_requested.set()

    def _emit(self, kind: str, value: object) -> None:
        self.messages.put((kind, value))

    def _close(self) -> None:
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except Exception:
                pass
            self.serial_port = None

    def _discover(self):
        from serial.tools import list_ports

        for port in list_ports.comports():
            if matches_r4_port(port):
                return port
        return None

    def _transact(self, command: str) -> str:
        if self.serial_port is None:
            raise ProtocolError("controller is not connected")
        if not command or len(command) > MAX_PROTOCOL_LINE or "\n" in command or "\r" in command:
            raise ProtocolError("invalid outgoing command")

        try:
            payload = (command + "\r\n").encode("ascii")
        except UnicodeEncodeError as error:
            raise ProtocolError("outgoing command is not ASCII") from error

        self.serial_port.write(payload)
        self.serial_port.flush()
        raw = self.serial_port.read_until(b"\n", MAX_PROTOCOL_LINE + 3)
        if not raw.endswith(b"\n"):
            raise ConnectionError(f"timeout waiting for {command!r}")
        try:
            response = raw.rstrip(b"\r\n").decode("ascii")
        except UnicodeDecodeError as error:
            raise ProtocolError("non-ASCII CDC response") from error
        if len(response) > MAX_PROTOCOL_LINE:
            raise ProtocolError("CDC response exceeds 255 characters")
        return response

    def _connect(self) -> bool:
        import serial

        port = self._discover()
        if port is None:
            self._emit("status", "Waiting for R4 Controller (CAFE:4005 / R4-0001)")
            return False

        self._emit("status", f"Opening {port.device}...")
        connection = serial.Serial(
            port=port.device,
            baudrate=115200,
            timeout=0.8,
            write_timeout=0.8,
        )
        connection.reset_input_buffer()
        connection.reset_output_buffer()
        self.serial_port = connection

        if self._transact("PING") != "PONG":
            raise ProtocolError("controller did not answer PONG")
        version = self._transact("VERSION")
        if not version.startswith("R4_CONTROLLER_FW "):
            raise ProtocolError(f"unexpected VERSION response: {version}")
        if self._transact("HOST HEARTBEAT") != "OK HOST HEARTBEAT":
            raise ProtocolError("controller rejected HOST HEARTBEAT")

        self._emit("version", version)
        self._emit("status", f"Connected: {port.device}")
        return True

    def run(self) -> None:
        try:
            import serial
        except ImportError:
            self._emit("fatal", "pyserial is missing; run: py -m pip install pyserial")
            return

        next_discovery = 0.0
        next_input = 0.0
        next_event = 0.0
        next_frame = 0.0
        next_heartbeat = 0.0
        last_input = ""
        last_frame_hash = None

        while not self.stop_requested.is_set():
            now = time.monotonic()

            if self.serial_port is None:
                if now < next_discovery:
                    self.stop_requested.wait(0.1)
                    continue
                next_discovery = now + 1.0
                try:
                    if not self._connect():
                        continue
                    next_input = next_event = next_frame = 0.0
                    next_heartbeat = time.monotonic() + 2.0
                    last_frame_hash = None
                except (serial.SerialException, OSError, ProtocolError) as error:
                    self._emit("error", str(error))
                    self._close()
                    continue

            try:
                processed_command = False
                while True:
                    try:
                        command = self.commands.get_nowait()
                    except queue.Empty:
                        break
                    response = self._transact(command)
                    if response.startswith("ERR "):
                        self._emit("error", response)
                    else:
                        self._emit("response", f"{command} -> {response}")
                    processed_command = True

                now = time.monotonic()
                if now >= next_heartbeat:
                    heartbeat = self._transact("HOST HEARTBEAT")
                    if heartbeat != "OK HOST HEARTBEAT":
                        raise ProtocolError(
                            f"unexpected heartbeat response: {heartbeat}"
                        )
                    next_heartbeat = now + 2.0

                if now >= next_input:
                    input_state = self._transact("INPUT")
                    if input_state != last_input:
                        last_input = input_state
                        self._emit("input", input_state)
                    next_input = now + 0.25

                if now >= next_event:
                    event = self._transact("EVENT NEXT")
                    if event != "EVENT NONE":
                        self._emit("event", event)
                    next_event = now + 0.15

                if processed_command or now >= next_frame:
                    frame = fetch_frame(self._transact, last_frame_hash)
                    if frame is not None:
                        last_frame_hash = frame.hash_value
                        self._emit("frame", frame)
                    next_frame = now + 0.10

                self.stop_requested.wait(0.02)
            except ProtocolError as error:
                self._emit("error", str(error))
                self.stop_requested.wait(0.2)
            except (serial.SerialException, OSError) as error:
                self._emit("error", f"connection lost: {error}")
                self._emit("status", "Disconnected; reconnecting...")
                self._close()
                next_discovery = time.monotonic() + 0.5

        self._close()


class TcpWorker(SerialWorker):
    def __init__(
        self,
        endpoint: TcpEndpoint,
        messages: queue.Queue[tuple[str, object]],
    ) -> None:
        super().__init__(messages)
        self.endpoint = endpoint
        self.tcp_socket: socket.socket | None = None
        self.tcp_file = None
        self.request_id = 0
        self.controller_ready = False

    def _close(self) -> None:
        self.controller_ready = False
        if self.tcp_file is not None:
            try:
                self.tcp_file.close()
            except OSError:
                pass
            self.tcp_file = None
        if self.tcp_socket is not None:
            try:
                self.tcp_socket.close()
            except OSError:
                pass
            self.tcp_socket = None

    def _connect(self) -> bool:
        self._emit(
            "status",
            "PC <-> Batocera: connecting | Batocera <-> RP2040: unknown",
        )
        connection = socket.create_connection(
            (self.endpoint.host, self.endpoint.port),
            timeout=3.0,
        )
        connection.settimeout(6.0)
        self.tcp_socket = connection
        self.tcp_file = connection.makefile("rwb", buffering=0)
        self._emit(
            "status",
            "PC <-> Batocera: connected | Batocera <-> RP2040: checking",
        )
        return True

    def _transact(self, command: str) -> str:
        if self.tcp_file is None:
            raise ConnectionError("Batocera TCP relay is not connected")

        self.request_id += 1
        if self.request_id > 2147483647:
            self.request_id = 1
        request_id = self.request_id
        self.tcp_file.write(format_request(request_id, command))
        raw = self.tcp_file.readline(2340)
        return parse_response(raw, request_id)

    def _verify_controller(self) -> None:
        if self._transact("PING") != "PONG":
            raise ProtocolError("RP2040 did not answer PONG")
        version = self._transact("VERSION")
        if not version.startswith("R4_CONTROLLER_FW "):
            raise ProtocolError(f"unexpected VERSION response: {version}")
        if self._transact("HOST HEARTBEAT") != "OK HOST HEARTBEAT":
            raise ProtocolError("RP2040 rejected HOST HEARTBEAT")

        self.controller_ready = True
        self._emit("version", version)
        self._emit(
            "status",
            "PC <-> Batocera: connected | Batocera <-> RP2040: connected",
        )

    def run(self) -> None:
        next_discovery = 0.0
        next_input = 0.0
        next_event = 0.0
        next_frame = 0.0
        next_heartbeat = 0.0
        last_input = ""
        last_frame_hash = None

        while not self.stop_requested.is_set():
            now = time.monotonic()

            if self.tcp_socket is None:
                if now < next_discovery:
                    self.stop_requested.wait(0.1)
                    continue
                next_discovery = now + 1.0
                try:
                    self._connect()
                except OSError as error:
                    self._emit("error", f"Batocera connection failed: {error}")
                    self._emit(
                        "status",
                        "PC <-> Batocera: disconnected | "
                        "Batocera <-> RP2040: unknown",
                    )
                    self._close()
                    continue

            if not self.controller_ready:
                try:
                    self._verify_controller()
                    next_input = next_event = next_frame = 0.0
                    next_heartbeat = time.monotonic() + 2.0
                    last_frame_hash = None
                except BridgeCommandError as error:
                    self._emit("error", str(error))
                    self._emit(
                        "status",
                        "PC <-> Batocera: connected | "
                        "Batocera <-> RP2040: unavailable",
                    )
                    self.stop_requested.wait(1.0)
                    continue
                except (OSError, ProtocolError) as error:
                    self._emit("error", str(error))
                    self._close()
                    next_discovery = time.monotonic() + 0.5
                    continue

            try:
                processed_command = False
                while True:
                    try:
                        command = self.commands.get_nowait()
                    except queue.Empty:
                        break
                    response = self._transact(command)
                    if response.startswith("ERR "):
                        self._emit("error", response)
                    else:
                        self._emit("response", f"{command} -> {response}")
                    processed_command = True

                now = time.monotonic()
                if now >= next_heartbeat:
                    heartbeat = self._transact("HOST HEARTBEAT")
                    if heartbeat != "OK HOST HEARTBEAT":
                        raise ProtocolError(
                            f"unexpected heartbeat response: {heartbeat}"
                        )
                    next_heartbeat = now + 2.0

                if now >= next_input:
                    input_state = self._transact("INPUT")
                    if input_state != last_input:
                        last_input = input_state
                        self._emit("input", input_state)
                    next_input = now + 0.25

                if now >= next_event:
                    event = self._transact("EVENT NEXT")
                    if event != "EVENT NONE":
                        self._emit("event", event)
                    next_event = now + 0.15

                if processed_command or now >= next_frame:
                    frame = fetch_network_frame(
                        self._transact,
                        last_frame_hash,
                    )
                    if frame is not None:
                        last_frame_hash = frame.hash_value
                        self._emit("frame", frame)
                    next_frame = now + 0.10

                self.stop_requested.wait(0.02)
            except BridgeCommandError as error:
                self.controller_ready = False
                self._emit("error", str(error))
                self._emit(
                    "status",
                    "PC <-> Batocera: connected | "
                    "Batocera <-> RP2040: unavailable",
                )
                self.stop_requested.wait(0.5)
            except (OSError, ProtocolError) as error:
                self._emit("error", f"TCP protocol/connection lost: {error}")
                self._emit(
                    "status",
                    "PC <-> Batocera: disconnected | "
                    "Batocera <-> RP2040: unknown",
                )
                self._close()
                next_discovery = time.monotonic() + 0.5

        self._close()


class OfflineRenderer:
    def __init__(
        self,
        executable: Path,
        messages: queue.Queue[tuple[str, object]],
    ) -> None:
        self.executable = executable
        self.messages = messages
        self.temp_directory = tempfile.TemporaryDirectory(prefix="r4-oled-offline-")

    def start(self) -> None:
        self.messages.put(("status", f"Offline fallback: {self.executable}"))
        self.render("home")

    def stop(self) -> None:
        self.temp_directory.cleanup()

    def submit(self, command: str, scenario: str = "home") -> None:
        self.messages.put(("response", f"offline preview: {command}"))
        self.render(scenario)

    def render(self, scenario: str) -> None:
        def task() -> None:
            try:
                subprocess.run(
                    [
                        str(self.executable),
                        "--output-dir",
                        self.temp_directory.name,
                        "--scenario",
                        scenario,
                    ],
                    check=True,
                    capture_output=True,
                    text=True,
                )
                path = Path(self.temp_directory.name) / f"{scenario}-128x64.pbm"
                self.messages.put(("frame", read_pbm(path)))
            except (OSError, subprocess.CalledProcessError, ProtocolError) as error:
                self.messages.put(("error", f"offline renderer: {error}"))

        threading.Thread(target=task, daemon=True).start()


class OledGui:
    def __init__(self, root: tk.Tk, args: argparse.Namespace) -> None:
        self.root = root
        self.root.title("R4 RP2040 OLED Emulator")
        self.messages: queue.Queue[tuple[str, object]] = queue.Queue()
        self.last_frame = None
        self.photo = None
        self.scaled_photo = None

        self.status = tk.StringVar(value="Starting...")
        self.version = tk.StringVar(value="Firmware: unknown")
        self.error = tk.StringVar(value="")
        self.input_state = tk.StringVar(value="Input: unknown")
        self.scale = tk.IntVar(value=args.scale)

        if args.offline:
            executable = resolve_headless_executable(args.headless_exe)
            self.transport = OfflineRenderer(executable, self.messages)
            self.offline = True
        elif args.tcp is not None:
            self.transport = TcpWorker(args.tcp, self.messages)
            self.offline = False
        else:
            self.transport = SerialWorker(self.messages)
            self.offline = False

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._close)
        self.transport.start()
        self.root.after(50, self._poll_messages)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=8)
        outer.grid(sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        header = ttk.Frame(outer)
        header.grid(row=0, column=0, columnspan=2, sticky="ew")
        ttk.Label(header, textvariable=self.status).pack(side="left")
        ttk.Label(header, textvariable=self.version).pack(side="right")

        self.canvas = tk.Canvas(outer, background="#202020", highlightthickness=0)
        self.canvas.grid(row=1, column=0, padx=(0, 8), pady=8, sticky="n")

        controls = ttk.Frame(outer)
        controls.grid(row=1, column=1, pady=8, sticky="nsew")

        state = ttk.LabelFrame(controls, text="Host state", padding=6)
        state.pack(fill="x", pady=(0, 6))
        for label, mode, scenario in (
            ("Boot", "BOOT", "boot"),
            ("Waiting", "WAITING", "waiting"),
            ("Home", "HOME", "home"),
            ("Diagnostic", "DIAGNOSTIC", "diagnostic"),
            ("Error", "ERROR", "error"),
        ):
            ttk.Button(
                state,
                text=label,
                command=lambda m=mode, s=scenario: self._send(
                    f"HOST STATE MODE={m}", s
                ),
            ).pack(side="left", padx=2)

        game = ttk.LabelFrame(controls, text="Game", padding=6)
        game.pack(fill="x", pady=6)
        self.system_name = tk.StringVar(value="NES")
        self.game_name = tk.StringVar(value="SUPER MARIO")
        ttk.Entry(game, textvariable=self.system_name, width=12).grid(row=0, column=0)
        ttk.Entry(game, textvariable=self.game_name, width=24).grid(row=0, column=1, padx=4)
        ttk.Button(game, text="Start", command=self._game_start).grid(row=0, column=2)
        ttk.Button(
            game,
            text="Stop",
            command=lambda: self._send("HOST GAME ACTION=STOP", "home"),
        ).grid(row=0, column=3, padx=4)

        achievement = ttk.LabelFrame(controls, text="RetroAchievements", padding=6)
        achievement.pack(fill="x", pady=6)
        self.ra_active = tk.BooleanVar(value=True)
        self.achievement_id = tk.StringVar(value="143820")
        self.achievement_title = tk.StringVar(value="FIRST WIN")
        ttk.Checkbutton(
            achievement,
            text="RA active",
            variable=self.ra_active,
            command=self._set_ra,
        ).grid(row=0, column=0)
        ttk.Entry(achievement, textvariable=self.achievement_id, width=9).grid(row=0, column=1)
        ttk.Entry(achievement, textvariable=self.achievement_title, width=22).grid(row=0, column=2, padx=4)
        ttk.Button(achievement, text="Unlock", command=self._achievement).grid(row=0, column=3)

        telemetry = ttk.LabelFrame(controls, text="Telemetry", padding=6)
        telemetry.pack(fill="x", pady=6)
        self.battery = tk.StringVar(value="78")
        self.runtime_minutes = tk.StringVar(value="155")
        self.volume = tk.StringVar(value="65")
        self.power = tk.StringVar(value="BATTERY")
        self.network = tk.StringVar(value="UP")
        self.clock = tk.StringVar(value="12:34")
        self.temperature = tk.StringVar(value="42125")
        for column, (label, variable, width) in enumerate((
            ("Battery", self.battery, 5),
            ("Runtime min", self.runtime_minutes, 8),
            ("Volume", self.volume, 5),
            ("Power", self.power, 10),
            ("Network", self.network, 7),
            ("Time", self.clock, 7),
            ("milli-C", self.temperature, 8),
        )):
            ttk.Label(telemetry, text=label).grid(row=0, column=column)
            ttk.Entry(telemetry, textvariable=variable, width=width).grid(row=1, column=column, padx=2)
        ttk.Button(telemetry, text="Send", command=self._telemetry).grid(row=1, column=7, padx=4)

        display = ttk.Frame(controls)
        display.pack(fill="x", pady=6)
        ttk.Label(display, text="Integer scale:").pack(side="left")
        ttk.Spinbox(
            display,
            from_=1,
            to=10,
            width=4,
            textvariable=self.scale,
            command=self._redraw,
        ).pack(side="left", padx=4)

        ttk.Label(outer, textvariable=self.input_state, wraplength=900).grid(
            row=2, column=0, columnspan=2, sticky="w"
        )
        ttk.Label(outer, textvariable=self.error, foreground="#b00020", wraplength=900).grid(
            row=3, column=0, columnspan=2, sticky="w"
        )
        self.log = tk.Text(outer, height=7, width=110, state="disabled")
        self.log.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(6, 0))

    def _send(self, command: str, scenario: str = "home") -> None:
        self.error.set("")
        if self.offline:
            self.transport.submit(command, scenario)
        else:
            self.transport.submit(command)

    def _game_start(self) -> None:
        self._send(
            "HOST GAME ACTION=START "
            f"SYSTEM_HEX={encode_text(self.system_name.get())} "
            f"GAME_HEX={encode_text(self.game_name.get())}",
            "game",
        )

    def _set_ra(self) -> None:
        self._send(f"HOST RA ACTIVE={1 if self.ra_active.get() else 0}", "home")

    def _achievement(self) -> None:
        identifier = self.achievement_id.get().strip() or "0"
        self._send(
            f"HOST ACHIEVEMENT ID={identifier} "
            f"TITLE_HEX={encode_text(self.achievement_title.get())}",
            "achievement",
        )

    def _telemetry(self) -> None:
        self._send(
            "HOST TELEMETRY "
            f"BATTERY={self.battery.get().strip()} "
            f"RUNTIME_MIN={self.runtime_minutes.get().strip()} "
            f"VOLUME={self.volume.get().strip()} "
            f"POWER={self.power.get().strip().upper()} "
            f"NETWORK={self.network.get().strip().upper()} "
            f"TEMP_MILLIC={self.temperature.get().strip()} "
            f"TIME_HEX={encode_text(self.clock.get())}",
            "home",
        )

    def _append_log(self, text: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _show_frame(self, frame) -> None:
        self.last_frame = frame
        scale = max(1, min(10, int(self.scale.get())))
        colors = tuple(
            tuple(
                "#ffffff" if frame.pixels[y * frame.width + x] else "#000000"
                for x in range(frame.width)
            )
            for y in range(frame.height)
        )
        self.photo = tk.PhotoImage(width=frame.width, height=frame.height)
        self.photo.put(colors)
        self.scaled_photo = self.photo.zoom(scale, scale)
        self.canvas.configure(
            width=frame.width * scale,
            height=frame.height * scale,
        )
        self.canvas.delete("all")
        self.canvas.create_image(0, 0, anchor="nw", image=self.scaled_photo)

    def _redraw(self) -> None:
        if self.last_frame is not None:
            self._show_frame(self.last_frame)

    def _poll_messages(self) -> None:
        try:
            while True:
                kind, value = self.messages.get_nowait()
                if kind == "status":
                    self.status.set(str(value))
                elif kind == "version":
                    self.version.set(str(value))
                elif kind in {"error", "fatal"}:
                    self.error.set(str(value))
                    self._append_log(f"ERROR: {value}")
                elif kind == "input":
                    self.input_state.set(f"Input: {value}")
                elif kind in {"event", "response"}:
                    self._append_log(str(value))
                elif kind == "frame":
                    self._show_frame(value)
        except queue.Empty:
            pass
        self.root.after(50, self._poll_messages)

    def _close(self) -> None:
        self.transport.stop()
        self.root.destroy()


def resolve_headless_executable(argument: str | None) -> Path:
    if argument:
        candidate = Path(argument).resolve()
    else:
        base = Path(__file__).resolve().parent.parent / "build-host"
        candidate = base / "r4-oled-emulator.exe"
        if not candidate.exists():
            candidate = base / "r4-oled-emulator"
    if not candidate.is_file():
        raise SystemExit(
            "offline mode requires --headless-exe pointing to r4-oled-emulator"
        )
    return candidate


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--offline",
        action="store_true",
        help="use the headless renderer instead of RP2040",
    )
    mode.add_argument(
        "--tcp",
        type=parse_endpoint,
        metavar="IP:PORT",
        help="connect through the Batocera diagnostic TCP relay",
    )
    parser.add_argument("--headless-exe", help="path to r4-oled-emulator for --offline")
    parser.add_argument("--scale", type=int, default=5, choices=range(1, 11))
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    root = tk.Tk()
    OledGui(root, args)
    root.mainloop()


if __name__ == "__main__":
    main()
