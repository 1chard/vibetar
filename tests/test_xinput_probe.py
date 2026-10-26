import ctypes
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "xinput_probe", Path(__file__).resolve().parents[1] / "tools/xinput_probe.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProbeTests(unittest.TestCase):
    def test_guitar_profile_names(self):
        self.assertEqual(probe.profile_name(6), "Rock Band")
        self.assertEqual(probe.profile_name(7), "Guitar Hero")
        self.assertEqual(probe.profile_name(1), "outro perfil")
    def test_windows_abi_layout(self):
        self.assertEqual(ctypes.sizeof(probe.Gamepad), 12)
        self.assertEqual(ctypes.sizeof(probe.State), 16)
        self.assertEqual(ctypes.sizeof(probe.Capabilities), 20)
        self.assertEqual(probe.Gamepad.right_x.offset, 8)
        self.assertEqual(probe.Gamepad.right_y.offset, 10)
        self.assertEqual(probe.Capabilities.gamepad.offset, 4)
        self.assertEqual(probe.Capabilities.vibration.offset, 16)

    def test_guitar_inputs_decode_from_xinput_state(self):
        gamepad = probe.Gamepad.from_buffer_copy(bytes.fromhex(
            "03 d1 00 00 00 00 00 00 00 80 ff 7f"))
        text = probe.describe(gamepad)
        for value in ("verde", "amarelo", "azul", "laranja", "strum-cima",
                      "strum-baixo", "whammy=-32768", "tilt=32767"):
            self.assertIn(value, text)
        self.assertNotIn("vermelho", text)

    def test_optional_rock_band_inputs(self):
        gamepad = probe.Gamepad()
        gamepad.buttons = 0x104c
        gamepad.left_trigger = 171
        text = probe.describe(gamepad)
        for value in ("verde", "esquerda", "direita", "solo", "LT=171"):
            self.assertIn(value, text)


if __name__ == "__main__":
    unittest.main()
