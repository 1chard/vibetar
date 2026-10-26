"""Read-only XInput diagnostic for Guitar Hero / Rock Band (Windows, no packages)."""
import argparse
import ctypes
import sys
import time


class Gamepad(ctypes.Structure):
    _fields_ = [("buttons", ctypes.c_uint16), ("left_trigger", ctypes.c_uint8),
                ("right_trigger", ctypes.c_uint8), ("left_x", ctypes.c_int16),
                ("left_y", ctypes.c_int16), ("right_x", ctypes.c_int16),
                ("right_y", ctypes.c_int16)]


class State(ctypes.Structure):
    _fields_ = [("packet", ctypes.c_uint32), ("gamepad", Gamepad)]


class Vibration(ctypes.Structure):
    _fields_ = [("left_motor", ctypes.c_uint16), ("right_motor", ctypes.c_uint16)]


class Capabilities(ctypes.Structure):
    _fields_ = [("type", ctypes.c_uint8), ("subtype", ctypes.c_uint8),
                ("flags", ctypes.c_uint16), ("gamepad", Gamepad), ("vibration", Vibration)]


BUTTONS = ((0x1000, "verde"), (0x2000, "vermelho"), (0x8000, "amarelo"),
           (0x4000, "azul"), (0x0100, "laranja"), (0x0001, "strum-cima"),
           (0x0002, "strum-baixo"), (0x0004, "esquerda"), (0x0008, "direita"),
           (0x0040, "solo"), (0x0010, "start"), (0x0020, "back"))


def describe(gamepad):
    pressed = ",".join(name for mask, name in BUTTONS if gamepad.buttons & mask) or "nenhum"
    return (f"botoes={pressed} whammy={gamepad.right_x} tilt={gamepad.right_y} "
            f"LX={gamepad.left_x} LY={gamepad.left_y} "
            f"LT={gamepad.left_trigger} RT={gamepad.right_trigger}")


def profile_name(subtype):
    return {6: "Rock Band", 7: "Guitar Hero"}.get(subtype, "outro perfil")


def load_xinput():
    if sys.platform != "win32":
        raise RuntimeError("Este utilitario usa XInput no Windows.")
    # xinput9_1_0 returns a fixed gamepad subtype; it cannot verify a guitar.
    for name in ("xinput1_4.dll", "xinput1_3.dll"):
        try:
            library = ctypes.WinDLL(name)
        except OSError:
            continue
        library.XInputGetCapabilities.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                                  ctypes.POINTER(Capabilities)]
        library.XInputGetCapabilities.restype = ctypes.c_uint32
        library.XInputGetState.argtypes = [ctypes.c_uint32, ctypes.POINTER(State)]
        library.XInputGetState.restype = ctypes.c_uint32
        return library
    raise RuntimeError("XInput 1.4/1.3 nao encontrado.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", type=int, choices=range(4), help="Slot XInput (0 a 3)")
    parser.add_argument("--watch", action="store_true", help="Monitorar ate Ctrl+C")
    args = parser.parse_args()
    try:
        library = load_xinput()
    except RuntimeError as error:
        parser.exit(1, f"{error}\n")
    indices = range(4) if args.index is None else (args.index,)
    last = {}
    try:
        while True:
            found = False
            for index in indices:
                caps, state = Capabilities(), State()
                cap_result = library.XInputGetCapabilities(index, 0, ctypes.byref(caps))
                state_result = library.XInputGetState(index, ctypes.byref(state))
                if cap_result == 1167 or state_result == 1167:
                    message = f"slot {index}: desconectado"
                elif cap_result or state_result:
                    message = f"slot {index}: erro capabilities={cap_result} state={state_result}"
                else:
                    found = True
                    profile = profile_name(caps.subtype)
                    message = (f"slot {index}: subtipo=0x{caps.subtype:02X} ({profile}) "
                               f"{describe(state.gamepad)}")
                if last.get(index) != message:
                    print(message, flush=True)
                    last[index] = message
            if not args.watch:
                return 0 if found else 1
            time.sleep(0.1)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
