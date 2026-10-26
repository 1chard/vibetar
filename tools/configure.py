"""Read/update Pico W settings using HID feature reports (no driver replacement)."""
import argparse
import json
import struct
import sys
import time

VID, PID = 0xCAFE, 0x4005
REPORT_ID, PAYLOAD_SIZE = 2, 24
BUTTON_NAMES = (
    "green", "red", "yellow", "blue", "orange", "strum_up", "strum_down",
    "start", "back", "tilt", "guide",
)
STATUS = {0: "ready", 1: "pending", 2: "invalid configuration", 3: "flash write failed"}


def integer(value, name, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{name}: use um inteiro entre {minimum} e {maximum}")
    return value


def validate(config):
    if not isinstance(config, dict) or set(config) != {"buttons", "mode_gpio", "whammy"}:
        raise ValueError("O JSON deve conter somente buttons, mode_gpio e whammy")
    buttons, whammy = config["buttons"], config["whammy"]
    if not isinstance(buttons, dict) or set(buttons) != set(BUTTON_NAMES):
        raise ValueError("buttons deve conter: " + ", ".join(BUTTON_NAMES))
    pins = [integer(buttons[name], name, 0, 22) for name in BUTTON_NAMES
            if not (name == "tilt" and type(buttons[name]) is int and buttons[name] == 255)]
    mode = integer(config["mode_gpio"], "mode_gpio", 0, 22)
    if len(set(pins + [mode])) != len(pins) + 1:
        raise ValueError("Todos os GPIOs dos botoes e MODE devem ser distintos")
    if not isinstance(whammy, dict) or set(whammy) != {"gpio", "enabled", "inverted", "min", "max", "deadzone"}:
        raise ValueError("whammy deve conter gpio, enabled, inverted, min, max e deadzone")
    integer(whammy["gpio"], "whammy.gpio", 26, 28)
    for name in ("enabled", "inverted"):
        if type(whammy[name]) is not bool:
            raise ValueError(f"whammy.{name} deve ser true ou false")
    minimum = integer(whammy["min"], "whammy.min", 0, 4095)
    maximum = integer(whammy["max"], "whammy.max", 0, 4095)
    deadzone = integer(whammy["deadzone"], "whammy.deadzone", 0, 4095)
    if minimum >= maximum or deadzone >= maximum - minimum:
        raise ValueError("Whammy exige min < max e deadzone < max - min")


def encode(config):
    validate(config)
    w = config["whammy"]
    p = bytearray(PAYLOAD_SIZE)
    p[:2] = b"\x01\x00"
    p[2:13] = bytes(config["buttons"][name] for name in BUTTON_NAMES)
    p[13:16] = bytes((config["mode_gpio"], w["gpio"], int(w["enabled"]) | int(w["inverted"]) << 1))
    struct.pack_into("<HHH", p, 16, w["min"], w["max"], w["deadzone"])
    return bytes([REPORT_ID]) + p


def decode(report, report_id=REPORT_ID):
    if len(report) != PAYLOAD_SIZE + 1 or report[0] != report_id:
        raise ValueError("Feature report incompleto ou firmware incompativel")
    p = bytes(report[1:])
    if p[0] != 1 or p[1] not in STATUS or p[15] & ~3 or any(p[22:]):
        raise ValueError("Versao ou formato do protocolo incompativel")
    minimum, maximum, deadzone = struct.unpack_from("<HHH", p, 16)
    config = {
        "buttons": dict(zip(BUTTON_NAMES, p[2:13])),
        "mode_gpio": p[13],
        "whammy": {"gpio": p[14], "enabled": bool(p[15] & 1), "inverted": bool(p[15] & 2),
                   "min": minimum, "max": maximum, "deadzone": deadzone},
    }
    validate(config)
    return config, p[1]


def read_device(device):
    return decode(device.get_feature_report(REPORT_ID, PAYLOAD_SIZE + 1))


def write_device(device, report, expected=None):
    if device.send_feature_report(report) != len(report):
        raise OSError("O dispositivo nao recebeu o report completo")
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        config, status = read_device(device)
        if status == 0:
            if expected is not None and config != expected:
                # SET_REPORT ACK precedes the callback; allow firmware to process it.
                time.sleep(0.02)
                continue
            return config
        if status != 1:
            raise OSError(STATUS[status])
        time.sleep(0.02)
    raise TimeoutError("Sem confirmacao de gravacao na flash; releia a configuracao")


def open_device(serial):
    try:
        import hid  # provided by the 'hidapi' package, not the 'hid' package
    except ImportError as error:
        raise RuntimeError("Instale a dependencia: python -m pip install -r tools/requirements.txt") from error
    # Windows exposes each top-level collection separately; pick the vendor one.
    candidates = [d for d in hid.enumerate(VID, PID)
                  if d.get("usage_page") == 0xFF00 and d.get("usage") == 1
                  and (serial is None or d.get("serial_number") == serial)]
    if not candidates:
        raise RuntimeError("Pico nao encontrado. Conecte o novo firmware em modo HID; use MODE para sair de XInput")
    if len(candidates) != 1:
        raise RuntimeError("Mais de um Pico encontrado. Escolha com --serial: " +
                           ", ".join(d.get("serial_number") or "?" for d in candidates))
    device = hid.device()
    device.open_path(candidates[0]["path"])
    return device


def main(argv=None):
    parser = argparse.ArgumentParser(description="Configuracao persistente do Vibetar via USB HID")
    parser.add_argument("--serial", help="Serial HID da placa, incluindo o sufixo -H")
    commands = parser.add_subparsers(dest="command", required=True)
    read = commands.add_parser("read", help="Ler GPIOs e calibracao atuais")
    read.add_argument("--output", help="Salvar o JSON em um arquivo")
    write = commands.add_parser("write", help="Validar, salvar na flash e aplicar um JSON")
    write.add_argument("file")
    commands.add_parser("reset", help="Salvar e aplicar os padroes compilados no firmware")
    check = commands.add_parser("check", help="Validar JSON sem conectar a placa")
    check.add_argument("file")
    args = parser.parse_args(argv)
    config = None
    if args.command in ("write", "check"):
        with open(args.file, encoding="utf-8") as source:
            config = json.load(source)
        validate(config)
    if args.command == "check":
        print("Configuracao valida.")
        return
    device = open_device(args.serial)
    try:
        if args.command == "write":
            config = write_device(device, encode(config), config)
        elif args.command == "reset":
            defaults, _ = decode(device.get_feature_report(3, PAYLOAD_SIZE + 1), 3)
            report = bytes([REPORT_ID, 1, 1]) + bytes(PAYLOAD_SIZE - 2)
            config = write_device(device, report, defaults)
        else:
            config, status = read_device(device)
            if status != 0:
                print("Status: " + STATUS[status], file=sys.stderr)
        result = json.dumps(config, indent=2) + "\n"
        if args.command == "read" and args.output:
            with open(args.output, "w", encoding="utf-8") as destination:
                destination.write(result)
        print(result, end="")
    finally:
        device.close()


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, RuntimeError) as error:
        print("Erro: " + str(error), file=sys.stderr)
        sys.exit(1)
