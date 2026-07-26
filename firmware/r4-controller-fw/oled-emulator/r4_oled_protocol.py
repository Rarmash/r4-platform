"""Pure protocol helpers for the R4 OLED CDC bridge."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Protocol

USB_VID = 0xCAFE
USB_PID = 0x4005
USB_SERIAL = "R4-0001"
MAX_PROTOCOL_LINE = 255
MAX_FRAMEBUFFER_FULL_LINE = 2304


class ProtocolError(RuntimeError):
    pass


class PortInfo(Protocol):
    vid: int | None
    pid: int | None
    serial_number: str | None


@dataclass(frozen=True)
class FrameInfo:
    snapshot_id: int
    width: int
    height: int
    byte_count: int
    hash_value: int
    chunk_max: int


@dataclass(frozen=True)
class Frame:
    width: int
    height: int
    pixels: bytes
    hash_value: int


def encode_text(value: str) -> str:
    return value.encode("utf-8").hex()


def matches_r4_port(port: PortInfo) -> bool:
    return (
        port.vid == USB_VID
        and port.pid == USB_PID
        and (port.serial_number or "").upper() == USB_SERIAL
    )


def parse_fields(line: str, prefix: str) -> dict[str, str]:
    if len(line) > MAX_PROTOCOL_LINE:
        raise ProtocolError("response exceeds 255 characters")
    if line != prefix and not line.startswith(prefix + " "):
        raise ProtocolError(f"expected {prefix!r}, got {line!r}")

    fields: dict[str, str] = {}
    remainder = line[len(prefix):].strip()
    for token in remainder.split():
        if "=" not in token:
            raise ProtocolError(f"malformed field {token!r}")
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            raise ProtocolError(f"invalid field {token!r}")
        fields[key] = value
    return fields


def _decimal(fields: dict[str, str], key: str) -> int:
    try:
        value = fields[key]
    except KeyError as error:
        raise ProtocolError(f"missing field {key}") from error
    if not value.isdecimal():
        raise ProtocolError(f"field {key} is not decimal")
    return int(value)


def parse_frame_info(line: str) -> FrameInfo:
    fields = parse_fields(line, "FRAMEBUFFER INFO")
    if fields.get("FORMAT") != "MONO1_MSB":
        raise ProtocolError("unsupported framebuffer format")

    try:
        hash_value = int(fields["HASH"], 16)
    except (KeyError, ValueError) as error:
        raise ProtocolError("invalid framebuffer hash") from error

    info = FrameInfo(
        snapshot_id=_decimal(fields, "ID"),
        width=_decimal(fields, "WIDTH"),
        height=_decimal(fields, "HEIGHT"),
        byte_count=_decimal(fields, "BYTES"),
        hash_value=hash_value,
        chunk_max=_decimal(fields, "CHUNK_MAX"),
    )
    if info.snapshot_id <= 0:
        raise ProtocolError("invalid snapshot ID")
    if info.width <= 0 or info.height <= 0:
        raise ProtocolError("invalid framebuffer dimensions")
    if info.width * info.height % 8:
        raise ProtocolError("framebuffer is not byte aligned")
    if info.byte_count != info.width * info.height // 8:
        raise ProtocolError("framebuffer byte count mismatch")
    if info.chunk_max <= 0 or info.chunk_max > 96:
        raise ProtocolError("unsafe chunk size")
    return info


def parse_frame_chunk(
    line: str,
    snapshot_id: int,
    offset: int,
    requested_length: int,
) -> bytes:
    fields = parse_fields(line, "FRAMEBUFFER DATA")
    if _decimal(fields, "ID") != snapshot_id:
        raise ProtocolError("snapshot ID changed during transfer")
    if _decimal(fields, "OFFSET") != offset:
        raise ProtocolError("chunk offset mismatch")
    if _decimal(fields, "LENGTH") != requested_length:
        raise ProtocolError("chunk length mismatch")
    try:
        data = bytes.fromhex(fields["HEX"])
    except (KeyError, ValueError) as error:
        raise ProtocolError("invalid chunk hex payload") from error
    if len(data) != requested_length:
        raise ProtocolError("decoded chunk length mismatch")
    return data


def unpack_mono1_msb(packed: bytes, pixel_count: int) -> bytes:
    if len(packed) * 8 != pixel_count:
        raise ProtocolError("packed framebuffer size mismatch")
    return bytes(
        1 if byte & (0x80 >> bit) else 0
        for byte in packed
        for bit in range(8)
    )


def framebuffer_hash(pixels: bytes) -> int:
    value = 2166136261
    for pixel in pixels:
        value ^= pixel
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def fetch_frame(
    transact: Callable[[str], str],
    previous_hash: int | None = None,
) -> Frame | None:
    info = parse_frame_info(transact("FRAMEBUFFER INFO"))
    if previous_hash is not None and info.hash_value == previous_hash:
        return None
    packed = bytearray()

    while len(packed) < info.byte_count:
        offset = len(packed)
        length = min(info.chunk_max, info.byte_count - offset)
        response = transact(
            f"FRAMEBUFFER CHUNK ID={info.snapshot_id} "
            f"OFFSET={offset} LENGTH={length}"
        )
        packed.extend(
            parse_frame_chunk(
                response,
                info.snapshot_id,
                offset,
                length,
            )
        )

    pixels = unpack_mono1_msb(bytes(packed), info.width * info.height)
    actual_hash = framebuffer_hash(pixels)
    if actual_hash != info.hash_value:
        raise ProtocolError(
            f"framebuffer hash mismatch: expected {info.hash_value:08X}, "
            f"got {actual_hash:08X}"
        )
    return Frame(info.width, info.height, pixels, actual_hash)


def fetch_network_frame(
    transact: Callable[[str], str],
    previous_hash: int | None = None,
) -> Frame | None:
    command = "FRAMEBUFFER READ"
    if previous_hash is not None:
        command += f" HASH={previous_hash:08X}"

    response = transact(command)
    if response.startswith("FRAMEBUFFER UNCHANGED "):
        fields = parse_fields(response, "FRAMEBUFFER UNCHANGED")
        try:
            unchanged_hash = int(fields["HASH"], 16)
        except (KeyError, ValueError) as error:
            raise ProtocolError("invalid unchanged framebuffer hash") from error
        if previous_hash is None or unchanged_hash != previous_hash:
            raise ProtocolError("unexpected unchanged framebuffer hash")
        return None

    if (
        not response.startswith("FRAMEBUFFER FULL ")
        or len(response) > MAX_FRAMEBUFFER_FULL_LINE
    ):
        raise ProtocolError("invalid full framebuffer response")

    fields: dict[str, str] = {}
    for token in response[len("FRAMEBUFFER FULL "):].split():
        if "=" not in token:
            raise ProtocolError("malformed full framebuffer field")
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            raise ProtocolError("invalid full framebuffer field")
        fields[key] = value

    if fields.get("FORMAT") != "MONO1_MSB":
        raise ProtocolError("unsupported full framebuffer format")

    try:
        width = int(fields["WIDTH"])
        height = int(fields["HEIGHT"])
        byte_count = int(fields["BYTES"])
        expected_hash = int(fields["HASH"], 16)
        packed = bytes.fromhex(fields["HEX"])
    except (KeyError, ValueError) as error:
        raise ProtocolError("invalid full framebuffer fields") from error

    if (
        width <= 0
        or height <= 0
        or width * height % 8
        or byte_count != width * height // 8
        or len(packed) != byte_count
    ):
        raise ProtocolError("full framebuffer geometry mismatch")

    pixels = unpack_mono1_msb(packed, width * height)
    actual_hash = framebuffer_hash(pixels)
    if actual_hash != expected_hash:
        raise ProtocolError(
            f"full framebuffer hash mismatch: "
            f"expected {expected_hash:08X}, got {actual_hash:08X}"
        )
    return Frame(width, height, pixels, actual_hash)


def read_pbm(path: str | Path) -> Frame:
    tokens = Path(path).read_text(encoding="ascii").split()
    if len(tokens) < 3 or tokens[0] != "P1":
        raise ProtocolError("offline renderer produced an invalid P1 PBM")
    width = int(tokens[1])
    height = int(tokens[2])
    values = tokens[3:]
    if len(values) != width * height or any(value not in {"0", "1"} for value in values):
        raise ProtocolError("offline PBM pixel count mismatch")
    pixels = bytes(int(value) for value in values)
    return Frame(width, height, pixels, framebuffer_hash(pixels))
