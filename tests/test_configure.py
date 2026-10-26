import copy
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("configure", ROOT / "tools/configure.py")
configure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(configure)


class ConfigTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((ROOT / "tools/config.example.json").read_text())

    def test_roundtrip_and_wire_layout(self):
        self.config["whammy"].update(min=237, max=3815, enabled=True, inverted=True)
        encoded = configure.encode(self.config)
        self.assertEqual(encoded[:3], bytes((2, 1, 0)))
        self.assertEqual(encoded[16], 3)
        self.assertEqual(encoded[17:23], bytes.fromhex("ed 00 e7 0e 20 00"))
        decoded, status = configure.decode(encoded)
        self.assertEqual(decoded, self.config)
        self.assertEqual(status, 0)

    def test_invalid_pin_and_calibration_rejected(self):
        cases = [
            ("duplicate", lambda c: c["buttons"].update(red=2)),
            ("mode collision", lambda c: c.update(mode_gpio=2)),
            ("reserved GPIO", lambda c: c["buttons"].update(green=23)),
            ("analog GPIO", lambda c: c["whammy"].update(gpio=29)),
            ("wrong type", lambda c: c["buttons"].update(green=True)),
            ("equal limits", lambda c: c["whammy"].update(min=4095)),
            ("out of ADC range", lambda c: c["whammy"].update(max=4096)),
            ("large deadzone", lambda c: c["whammy"].update(deadzone=4095)),
            ("unknown field", lambda c: c.update(unknown=1)),
        ]
        for name, mutate in cases:
            with self.subTest(name=name):
                c = copy.deepcopy(self.config)
                mutate(c)
                with self.assertRaises(ValueError):
                    configure.encode(c)

    def test_optional_tilt_button(self):
        self.config["buttons"]["tilt"] = 255
        decoded, status = configure.decode(configure.encode(self.config))
        self.assertEqual(decoded, self.config)
        self.assertEqual(status, 0)
        self.config["buttons"]["green"] = 255
        with self.assertRaises(ValueError):
            configure.encode(self.config)

    def test_protocol_errors(self):
        valid = configure.encode(self.config)
        for report in (valid[:-1], bytes([4]) + valid[1:], valid[:1] + bytes([9]) + valid[2:],
                       valid[:23] + b"\x00\x01"):
            with self.assertRaises(ValueError):
                configure.decode(report)

    def test_error_status_keeps_old_configuration_readable(self):
        report = bytearray(configure.encode(self.config))
        report[2] = 3
        config, status = configure.decode(report)
        self.assertEqual(config, self.config)
        self.assertEqual(status, 3)

    def test_save_waits_for_confirmation(self):
        target = copy.deepcopy(self.config)
        target["whammy"]["min"] = 100
        old_ready = configure.encode(self.config)
        pending = bytearray(old_ready)
        pending[2] = 1
        replies = iter((old_ready, pending, configure.encode(target)))

        class Device:
            def send_feature_report(self, report):
                return len(report)

            def get_feature_report(self, report_id, length):
                return next(replies)

        with patch.object(configure.time, "sleep"):
            self.assertEqual(configure.write_device(Device(), configure.encode(target), target), target)

    def test_flash_error_and_short_write_are_not_success(self):
        report = bytearray(configure.encode(self.config))
        report[2] = 3

        class Device:
            def send_feature_report(self, data):
                return len(data)

            def get_feature_report(self, report_id, length):
                return report

        with self.assertRaisesRegex(OSError, "flash write failed"):
            configure.write_device(Device(), configure.encode(self.config), self.config)
        with patch.object(Device, "send_feature_report", return_value=0):
            with self.assertRaises(OSError):
                configure.write_device(Device(), configure.encode(self.config), self.config)

    def test_vendor_collection_selection(self):
        import types
        opened = []
        fake_device = types.SimpleNamespace(open_path=opened.append)
        fake_hid = types.SimpleNamespace(
            enumerate=lambda vid, pid: [
                {"usage_page": 1, "usage": 5, "path": b"gamepad", "serial_number": "ABC-H"},
                {"usage_page": 0xFF00, "usage": 1, "path": b"config", "serial_number": "ABC-H"},
            ], device=lambda: fake_device,
        )
        with patch.dict("sys.modules", {"hid": fake_hid}):
            self.assertIs(configure.open_device("ABC-H"), fake_device)
            self.assertEqual(opened, [b"config"])
            with self.assertRaises(RuntimeError):
                configure.open_device("OTHER-H")


if __name__ == "__main__":
    unittest.main()
