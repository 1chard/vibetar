# Mode shortcuts

Hold Start + Back/Select continuously for 3 seconds to force USB HID. This
overrides automatic Xbox detection and XInput-only policy for the session,
without changing saved settings. The Bluetooth receiver recognizes the same
held buttons received from the guitar; update both firmware images.

In the editor's Host section, enable the Start + Select mode-switch option on
the guitar and save. Releasing a short chord cycles the allowed modes once,
including through the existing Bluetooth mode counter on the receiver. A long
hold forces HID only, without a second mode change when released. MODE remains
available. The optional shortcut defaults off and is stored in flash.

Feature 12/13 keeps its 8-byte version-1 layout. Byte 4 is now chord_toggle
(0/1); bytes 5–7 remain reserved zero. Old JSON without chord_toggle and old
flash records default to false. JSON exports preserve the enabled option.

Start + Back + MODE no longer enters BOOTSEL. Use the editor's BOOTSEL command
or the board's physical BOOTSEL button to replace firmware.
