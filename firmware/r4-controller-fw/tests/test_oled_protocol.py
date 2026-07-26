from __future__ import annotations

import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

EMULATOR_DIR = Path(__file__).resolve().parents[1] / "oled-emulator"
sys.path.insert(0, str(EMULATOR_DIR))

from r4_oled_protocol import (  # noqa: E402
    ProtocolError,
    encode_text,
    fetch_frame,
    fetch_network_frame,
    framebuffer_hash,
    matches_r4_port,
    parse_frame_chunk,
    parse_frame_info,
)


@dataclass
class FakePort:
    vid: int | None
    pid: int | None
    serial_number: str | None


class ProtocolTests(unittest.TestCase):
    def test_matches_stable_usb_identity(self) -> None:
        self.assertTrue(matches_r4_port(FakePort(0xCAFE, 0x4005, "R4-0001")))
        self.assertFalse(matches_r4_port(FakePort(0xCAFE, 0x4005, "OTHER")))
        self.assertFalse(matches_r4_port(FakePort(None, None, None)))

    def test_text_encoding(self) -> None:
        self.assertEqual(encode_text("NES"), "4e4553")
        self.assertEqual(encode_text("??????"), "??????".encode().hex())

    def test_frame_info_validation(self) -> None:
        info = parse_frame_info(
            "FRAMEBUFFER INFO ID=4 WIDTH=128 HEIGHT=64 "
            "FORMAT=MONO1_MSB BYTES=1024 HASH=867EBAEA CHUNK_MAX=96"
        )
        self.assertEqual(info.snapshot_id, 4)
        self.assertEqual(info.byte_count, 1024)
        with self.assertRaises(ProtocolError):
            parse_frame_info(
                "FRAMEBUFFER INFO ID=4 WIDTH=128 HEIGHT=64 "
                "FORMAT=RAW BYTES=1024 HASH=0 CHUNK_MAX=96"
            )

    def test_chunk_validation(self) -> None:
        response = "FRAMEBUFFER DATA ID=9 OFFSET=0 LENGTH=2 HEX=81FF"
        self.assertEqual(parse_frame_chunk(response, 9, 0, 2), b"\x81\xff")
        with self.assertRaises(ProtocolError):
            parse_frame_chunk(response, 8, 0, 2)
        with self.assertRaises(ProtocolError):
            parse_frame_chunk(response, 9, 0, 3)

    def test_fetches_and_verifies_complete_frame(self) -> None:
        packed = bytes((0x81, 0xFF))
        pixels = bytes(
            1 if byte & (0x80 >> bit) else 0
            for byte in packed
            for bit in range(8)
        )
        expected_hash = framebuffer_hash(pixels)
        commands: list[str] = []

        def transact(command: str) -> str:
            commands.append(command)
            if command == "FRAMEBUFFER INFO":
                return (
                    "FRAMEBUFFER INFO ID=3 WIDTH=8 HEIGHT=2 "
                    f"FORMAT=MONO1_MSB BYTES=2 HASH={expected_hash:08X} CHUNK_MAX=1"
                )
            if "OFFSET=0" in command:
                return "FRAMEBUFFER DATA ID=3 OFFSET=0 LENGTH=1 HEX=81"
            return "FRAMEBUFFER DATA ID=3 OFFSET=1 LENGTH=1 HEX=FF"

        frame = fetch_frame(transact)
        self.assertEqual(frame.width, 8)
        self.assertEqual(frame.height, 2)
        self.assertEqual(frame.pixels, pixels)
        self.assertEqual(len(commands), 3)

    def test_rejects_hash_mismatch(self) -> None:
        def transact(command: str) -> str:
            if command == "FRAMEBUFFER INFO":
                return (
                    "FRAMEBUFFER INFO ID=1 WIDTH=8 HEIGHT=1 "
                    "FORMAT=MONO1_MSB BYTES=1 HASH=00000000 CHUNK_MAX=1"
                )
            return "FRAMEBUFFER DATA ID=1 OFFSET=0 LENGTH=1 HEX=00"

        with self.assertRaises(ProtocolError):
            fetch_frame(transact)

    def test_unchanged_hash_skips_all_chunks(self) -> None:
        commands: list[str] = []

        def transact(command: str) -> str:
            commands.append(command)
            return (
                "FRAMEBUFFER INFO ID=8 WIDTH=8 HEIGHT=1 "
                "FORMAT=MONO1_MSB BYTES=1 HASH=12345678 CHUNK_MAX=1"
            )

        self.assertIsNone(fetch_frame(transact, 0x12345678))
        self.assertEqual(commands, ["FRAMEBUFFER INFO"])

    def test_network_frame_uses_one_aggregate_request(self) -> None:
        packed = bytes((0x81, 0xFF))
        pixels = bytes(
            1 if byte & (0x80 >> bit) else 0
            for byte in packed
            for bit in range(8)
        )
        expected_hash = framebuffer_hash(pixels)
        commands: list[str] = []

        def transact(command: str) -> str:
            commands.append(command)
            return (
                "FRAMEBUFFER FULL WIDTH=8 HEIGHT=2 "
                f"FORMAT=MONO1_MSB BYTES=2 HASH={expected_hash:08X} "
                "HEX=81FF"
            )

        frame = fetch_network_frame(transact)
        self.assertEqual(frame.pixels, pixels)
        self.assertEqual(commands, ["FRAMEBUFFER READ"])

        def unchanged(command: str) -> str:
            commands.append(command)
            return f"FRAMEBUFFER UNCHANGED HASH={expected_hash:08X}"

        self.assertIsNone(fetch_network_frame(unchanged, expected_hash))
        self.assertEqual(
            commands[-1],
            f"FRAMEBUFFER READ HASH={expected_hash:08X}",
        )


if __name__ == "__main__":
    unittest.main()
