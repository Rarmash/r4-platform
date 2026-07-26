from __future__ import annotations

import sys
import tkinter as tk
import unittest
from pathlib import Path

EMULATOR_DIR = Path(__file__).resolve().parents[1] / "oled-emulator"
sys.path.insert(0, str(EMULATOR_DIR))

from r4_oled_gui import OledGui  # noqa: E402
from r4_oled_protocol import Frame  # noqa: E402


class TkFramebufferTests(unittest.TestCase):
    def setUp(self) -> None:
        try:
            self.root = tk.Tk()
        except tk.TclError as error:
            self.skipTest(f"Tk display is unavailable: {error}")
        self.root.withdraw()

    def tearDown(self) -> None:
        if hasattr(self, "root"):
            self.root.destroy()

    def test_uploads_pixels_without_optional_image_formats(self) -> None:
        gui = OledGui.__new__(OledGui)
        gui.scale = tk.IntVar(self.root, value=3)
        gui.canvas = tk.Canvas(self.root)
        gui.last_frame = None
        gui.photo = None
        gui.scaled_photo = None

        gui._show_frame(
            Frame(
                width=2,
                height=2,
                pixels=bytes((0, 1, 1, 0)),
                hash_value=0,
            )
        )

        self.assertEqual(gui.photo.get(0, 0), (0, 0, 0))
        self.assertEqual(gui.photo.get(1, 0), (255, 255, 255))
        self.assertEqual(gui.photo.get(0, 1), (255, 255, 255))
        self.assertEqual(gui.photo.get(1, 1), (0, 0, 0))
        self.assertEqual(int(gui.canvas.cget("width")), 6)
        self.assertEqual(int(gui.canvas.cget("height")), 6)


if __name__ == "__main__":
    unittest.main()
