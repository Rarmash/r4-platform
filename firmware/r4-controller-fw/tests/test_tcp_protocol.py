from __future__ import annotations

import sys
import unittest
from pathlib import Path

EMULATOR_DIR = Path(__file__).resolve().parents[1] / "oled-emulator"
sys.path.insert(0, str(EMULATOR_DIR))

from r4_oled_protocol import ProtocolError  # noqa: E402
from r4_tcp_protocol import (  # noqa: E402
    BridgeCommandError,
    TcpEndpoint,
    format_request,
    parse_endpoint,
    parse_response,
)


class TcpProtocolTests(unittest.TestCase):
    def test_endpoint_parsing(self) -> None:
        self.assertEqual(
            parse_endpoint("192.168.1.123:4274"),
            TcpEndpoint("192.168.1.123", 4274),
        )
        self.assertEqual(
            parse_endpoint("[fe80::1]:4274"),
            TcpEndpoint("fe80::1", 4274),
        )
        for invalid in ("localhost", ":4274", "host:0", "host:65536", "a:b:1"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                parse_endpoint(invalid)

    def test_request_and_response_framing(self) -> None:
        self.assertEqual(format_request(17, "PING"), b"REQ 17 PING\n")
        self.assertEqual(parse_response(b"RES 17 OK PONG\n", 17), "PONG")
        full = (
            "FRAMEBUFFER FULL WIDTH=128 HEIGHT=64 "
            "FORMAT=MONO1_MSB BYTES=1024 HASH=00000000 HEX="
            + "00" * 1024
        )
        self.assertEqual(
            parse_response(f"RES 18 OK {full}\n".encode("ascii"), 18),
            full,
        )

    def test_rejects_malformed_or_mismatched_response(self) -> None:
        for response in (
            b"RES 2 OK PONG",
            b"GARBAGE\n",
            b"RES x OK PONG\n",
            b"RES 3 OK PONG\n",
            b"RES 2 MAYBE PONG\n",
        ):
            with self.subTest(response=response), self.assertRaises(
                (ConnectionError, ProtocolError)
            ):
                parse_response(response, 2)

    def test_bridge_error_is_distinct_from_transport_error(self) -> None:
        with self.assertRaisesRegex(BridgeCommandError, "RP2040_UNAVAILABLE"):
            parse_response(b"RES 5 ERROR RP2040_UNAVAILABLE\n", 5)

    def test_rejects_invalid_outgoing_command(self) -> None:
        for command in ("", "PING\nVERSION", "X" * 256, "ПИНГ"):
            with self.subTest(command=command), self.assertRaises(ProtocolError):
                format_request(1, command)


if __name__ == "__main__":
    unittest.main()
