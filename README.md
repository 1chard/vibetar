# Vibetar

**Give your rhythm-game guitar a new life.**

Vibetar is Raspberry Pi Pico firmware for DIY Guitar Hero and Rock Band
controllers. It reads your guitar's buttons and sensors and presents them as a
USB HID gamepad, a Guitar Hero XInput guitar, or a Rock Band XInput guitar.
A browser editor lets you change wiring, calibration, and settings without
rebuilding the firmware.

The project was developed with AI assistance. Its name combines the idea of
*vibe coding* with *guitar*.

## Features

- **Three USB modes:** generic HID, Guitar Hero XInput, and Rock Band XInput.
- **One guitar firmware for Pico and Pico W:** radio capabilities are detected at startup.
- **Optional Bluetooth HID on Pico W:** connect directly to a computer or use a second Pico W as a USB receiver.
- **Persistent configuration:** remap GPIOs and calibrate inputs in the Vibetar Editor.
- **Whammy and tilt:** digital tilt by default, with optional ADC or ADXL345 accelerometer support.
- **Extra controls:** solo frets, left/right D-pad, and an analog Rock Band effects selector.
- **Indicators and monitoring:** event-driven LEDs, optional VSYS low-battery monitoring, live GPIO readings, and a controller preview.
- **Recovery tools:** factory settings, JSON backups, software restart, and BOOTSEL access.
- **Xbox 360 authentication:** integrated XSM3 through libxsm3, without an external authentication controller.

## How it works

Connect normally open switches between GPIO and GND. Vibetar uses internal
pull-ups, reads inputs every 1 ms, applies 3 ms button debounce, and sends USB
reports at a nominal 4 ms interval. Settings are stored in flash and shared by
the USB profiles.

Press and release **MODE** to cycle through **HID → Guitar Hero → Rock Band**.
Each switch briefly disconnects USB for re-enumeration; the Pico keeps running.
Some games may need you to select the controller again.

The default **Automatic** host policy starts in HID. A positive Xbox 360 XSM3
signature selects XInput and limits MODE to the two guitar profiles. An unknown
host keeps all three modes available. If a console does not request XSM3 in HID,
select XInput manually or save the **XInput only** policy in the editor.

Bluetooth uses generic HID; Guitar Hero and Rock Band XInput are USB profiles.
With a Pico W receiver, the wireless link connects the two boards and the
computer or console receives a USB controller.

## Hardware and compatibility

| Device | Role |
| --- | --- |
| Raspberry Pi Pico (RP2040) | USB guitar controller |
| Raspberry Pi Pico W (RP2040) | USB guitar controller with optional Bluetooth HID |
| Second Raspberry Pi Pico W | Optional Bluetooth-to-USB receiver |
| Pico 2 / Pico 2 W | Not targeted by the current build |

The author has reported the USB controller working on PC and on both stock and
modified Xbox 360 consoles. Optional sensors, extra controls, the two-board
wireless link, and individual game combinations still need physical validation.
Automated tests cover logic, protocols, and compiled firmware layouts.

## Install the firmware

Build the firmware using the instructions below, or use a UF2 produced from the
same source revision if one has been provided.

1. Hold the board's **BOOTSEL** button while connecting it by USB.
2. The board appears as a drive named **RPI-RP2**.
3. Copy **`build/vibetar.uf2`** to that drive for a guitar, or
   **`build/vibetar-receiver.uf2`** for a receiver.
4. The board restarts automatically. Connect the guitar in HID mode to configure it.

The guitar UF2 works on both supported boards. The receiver requires Pico W.
Normal Vibetar UF2 updates preserve saved settings and Bluetooth bonds; a full
flash erase or another firmware may remove them.

### Default wiring

These are **GPIO numbers, not physical header pin numbers**. Connect each
switch between its GPIO and **GND**. GPIOs use **3.3 V**; never apply 5 V.

| Input | GPIO |
| --- | --- |
| Green / Red / Yellow / Blue / Orange | GP2 / GP3 / GP4 / GP5 / GP6 |
| Strum up / down | GP7 / GP8 |
| Start / Back (Select) | GP9 / GP10 |
| Tilt (Star Power) / Guide | GP11 / GP12 |
| MODE | GP13 |
| Optional whammy potentiometer | GP26 / ADC0 |

For whammy, connect the potentiometer ends to **3V3 and GND**, and its wiper to
GP26. Enable whammy in the editor and calibrate its resting and full-travel
values. It is disabled by default so an unconnected ADC does not produce input.
Digital inputs can be remapped to GP0–GP22; analog inputs use GP26–GP28. The
editor rejects conflicting assignments.

## Configure with Vibetar Editor

The companion editor is distributed as **Vibetar Editor** (`vibetar-web`).
From the editor checkout, run:

```sh
node server.mjs
```

Open [localhost:8080](http://localhost:8080) in Chrome or Edge. WebHID requires a
compatible browser and HTTPS or localhost.

1. Connect the board by USB in **HID** mode.
2. Click **Connect Pico** and authorize the device in the browser's picker.
3. Change GPIOs, calibration, or optional features.
4. Use the save button for each changed section. The editor confirms the saved values.

The editor supports English and Portuguese (Brazil). It remembers a manual
choice, otherwise uses a supported browser language, and falls back to English. Importing a JSON backup
creates a draft; saving writes it to the board. Configuration changes persist
after power-off. Make changes outside gameplay because flash writes briefly
pause input processing. No configurations are sent to an internet server.

### Bluetooth and receiver setup

Bluetooth is disabled by default on the guitar. Enable it through the USB editor,
then pair **Vibetar Guitar** in your computer's Bluetooth settings. New pairing
is available for 120 seconds after opening the pairing window. One Bluetooth
connection is supported at a time.

To use a receiver:

1. Flash the guitar and receiver UF2 files to their respective Pico W boards.
2. Enable Bluetooth on the guitar and open its pairing window.
3. Connect the receiver to the editor over USB HID. Use **Search guitars**,
   select the guitar, and choose **Connect guitar**.
4. Wait until the receiver reports that it is receiving controls. It then saves
   the peer and reconnects automatically on later startups.
5. Connect the receiver by USB to your computer or console. MODE on the guitar
   requests a USB mode change on the receiver.

Disconnect the guitar from the computer's Bluetooth connection before pairing
it with a receiver. If it also stays enumerated over USB, disable **Prioritize USB**
on the guitar to transmit real input over Bluetooth. Configure buttons and sensors
on the guitar; settings edited on the receiver apply to the receiver itself.

### Recovery and updates

- **Force HID:** hold Start + Back while powering on, without MODE. This restores editor access for that session.
- **Enter BOOTSEL:** hold Start + Back + MODE for 3 seconds, or use the editor's BOOTSEL command. Then copy a UF2 to RPI-RP2.
- **Restore settings:** use the editor's factory-reset action. BOOTSEL itself does not erase settings.

Button gestures use the saved pin mapping. The physical BOOTSEL button remains
available if the firmware is not running or the configured buttons are inaccessible.
After upgrading an older Bluetooth installation, you may need to remove the
previous pairing on the computer, clear bonds in the editor, and pair again.

## Build from source

Clone this repository and initialize its submodules:

```sh
git submodule update --init --recursive
```

The validated environment is **Pico SDK 2.2.0**, its bundled **TinyUSB 0.18.0**,
**ARM GCC 14.2.1**, CMake, Ninja, and Python. The SDK's TinyUSB, BTstack, and
CYW43 driver submodules are required. The build also needs picotool; the SDK can
fetch and build it when a local installation is unavailable.

### Windows

The Raspberry Pi Pico extension for VS Code can install the toolchain. With
its default installation paths:

```powershell
.\scripts\build.ps1
```

For a different setup:

```powershell
.\scripts\build.ps1 -SdkPath 'C:\pico\pico-sdk' -ToolchainPath 'C:\pico\arm-toolchain'
```

### CMake on Windows, Linux, or macOS

With the ARM toolchain, CMake, Ninja, and Python on PATH, prepare the SDK:

```sh
git clone --branch 2.2.0 --depth 1 https://github.com/raspberrypi/pico-sdk.git pico-sdk
git -C pico-sdk submodule update --init lib/tinyusb lib/btstack lib/cyw43-driver
cmake -S . -B build -G Ninja -DPICO_SDK_PATH="/absolute/path/to/pico-sdk" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
```

Both `build/vibetar.uf2` and `build/vibetar-receiver.uf2` are produced. The SDK
build target is `pico_w` for both; the guitar detects the physical board at boot.
For an offline build, provide an installed picotool using
`-Dpicotool_DIR=/path/to/its/cmake/config`.

### Optional Python configuration tool

USB HID mode also supports reading and writing the basic button/whammy settings:

```sh
python -m pip install -r tools/requirements.txt
python tools/configure.py read --output guitar.json
python tools/configure.py check guitar.json
python tools/configure.py write guitar.json
```

Edit the exported JSON before checking and writing it. Use the web editor for
extended features such as Bluetooth, LEDs, tilt sensors, and receiver setup.

## Development

Static C++ assertions are compiled with both firmware targets. After building:

```sh
python -m unittest discover -s tests -p 'test_*.py' -v
node --test tests/*.test.mjs
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for native authentication tests and bug
reports. Detailed hardware and protocol documentation is reserved for a future
GitHub Wiki; the repository README contains the setup instructions.

## License and credits

Original project code is licensed under the [MIT License](LICENSE).
Dependencies and adapted sources retain their own licenses; see
[third-party notices](THIRD_PARTY_NOTICES.md).

Thanks to the Raspberry Pi Pico SDK, TinyUSB, BTstack, and
[libxsm3](https://github.com/InvoxiPlayGames/libxsm3) projects.
Vibetar is an independent project and is not affiliated with the owners of
Guitar Hero, Rock Band, Xbox, or Raspberry Pi.
