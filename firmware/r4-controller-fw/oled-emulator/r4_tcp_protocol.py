"""Framing helpers for the Batocera OLED diagnostic TCP relay."""

from __future__ import annotations

from dataclasses import dataclass

from r4_oled_protocol import (
    MAX_FRAMEBUFFER_FULL_LINE,
    MAX_PROTOCOL_LINE,
    ProtocolError,
)

MAX_WIRE_LINE = 640
MAX_WIRE_RESPONSE = MAX_FRAMEBUFFER_FULL_LINE + 32


class BridgeCommandError(ProtocolError):
    pass


@dataclass(frozen=True)
class TcpEndpoint:
    host: str
    port: int


def parse_endpoint(value: str) -> TcpEndpoint:
    if value.startswith("["):
        closing = value.find("]")
        if closing < 0 or closing + 1 >= len(value) or value[closing + 1] != ":":
            raise ValueError("TCP endpoint must be [IPv6]:PORT")
        host = value[1:closing]
        port_text = value[closing + 2:]
    else:
        if value.count(":") != 1:
            raise ValueError("TCP endpoint must be IP:PORT")
        host, port_text = value.rsplit(":", 1)

    if not host:
        raise ValueError("TCP host is empty")
    if not port_text.isdecimal():
        raise ValueError("TCP port is not decimal")

    port = int(port_text)
    if port < 1 or port > 65535:
        raise ValueError("TCP port is outside 1..65535")
    return TcpEndpoint(host, port)


def format_request(request_id: int, command: str) -> bytes:
    if request_id < 1:
        raise ProtocolError("TCP request ID must be positive")
    if (
        not command
        or len(command) > MAX_PROTOCOL_LINE
        or "\r" in command
        or "\n" in command
    ):
        raise ProtocolError("invalid outgoing command")

    try:
        encoded = f"REQ {request_id} {command}\n".encode("ascii")
    except UnicodeEncodeError as error:
        raise ProtocolError("outgoing command is not ASCII") from error

    if len(encoded) > MAX_WIRE_LINE + 1:
        raise ProtocolError("TCP request exceeds framing limit")
    return encoded


def parse_response(raw: bytes, expected_request_id: int) -> str:
    if not raw.endswith(b"\n"):
        raise ConnectionError("truncated TCP response")
    if len(raw) > MAX_WIRE_RESPONSE + 1:
        raise ProtocolError("TCP response exceeds framing limit")

    try:
        line = raw.rstrip(b"\r\n").decode("ascii")
    except UnicodeDecodeError as error:
        raise ProtocolError("non-ASCII TCP response") from error

    parts = line.split(" ", 3)
    if len(parts) < 3 or parts[0] != "RES":
        raise ProtocolError(f"malformed TCP response: {line!r}")
    if not parts[1].isdecimal():
        raise ProtocolError("TCP response has invalid request ID")
    if int(parts[1]) != expected_request_id:
        raise ProtocolError("TCP response request ID mismatch")

    payload = parts[3] if len(parts) == 4 else ""
    if parts[2] == "ERROR":
        raise BridgeCommandError(payload or "Batocera relay rejected command")
    if parts[2] != "OK":
        raise ProtocolError("TCP response has invalid status")
    payload_limit = (
        MAX_FRAMEBUFFER_FULL_LINE
        if payload.startswith("FRAMEBUFFER FULL ")
        else MAX_PROTOCOL_LINE
    )
    if not payload or len(payload) > payload_limit:
        raise ProtocolError("invalid CDC payload in TCP response")
    return payload
