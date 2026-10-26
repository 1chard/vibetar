#pragma once

#include <array>
#include <cstdint>

namespace guitar::config {
// GPIO/calibration values below are factory defaults. Saved runtime settings
// override them; edit the running device using tools/configure.py in HID mode.
// GPIO numbers, not physical header pin numbers. Switches connect GPIO to GND.
inline constexpr std::array<uint8_t, 11> button_pins = {
    2, 3, 4, 5, 6, // green, red, yellow, blue, orange
    7, 8,         // strum up, down
    9, 10,        // start, back
    11, 12        // tilt / star power, guide
};
inline constexpr uint8_t mode_pin = 13;
inline constexpr uint8_t whammy_pin = 26;
inline constexpr uint8_t whammy_adc_channel = 0;
// Disabled by default so an unconnected analog pin never generates random input.
inline constexpr bool whammy_enabled = false;
inline constexpr uint16_t whammy_min = 0;
inline constexpr uint16_t whammy_max = 4095;
inline constexpr uint16_t whammy_deadzone = 32;
inline constexpr bool whammy_inverted = false;
inline constexpr uint32_t debounce_ms = 3;
inline constexpr uint32_t sample_interval_ms = 1;
inline constexpr uint32_t report_interval_ms = 4;
inline constexpr uint32_t usb_detach_ms = 350;
inline constexpr uint32_t bootsel_hold_ms = 3000;
// USB startup/cycling policy now lives in persistent HostConfig (host_policy.hpp).

constexpr bool valid_pins() {
    for (unsigned i = 0; i < button_pins.size(); ++i) {
        if (button_pins[i] > 22 || button_pins[i] == mode_pin) return false;
        for (unsigned j = i + 1; j < button_pins.size(); ++j)
            if (button_pins[i] == button_pins[j]) return false;
    }
    return mode_pin <= 22 && whammy_pin >= 26 && whammy_pin <= 28 &&
           whammy_adc_channel == whammy_pin - 26;
}
static_assert(valid_pins(), "Use distinct exposed Pico W GPIOs and a matching ADC channel");
static_assert(whammy_min < whammy_max && whammy_max <= 4095);
static_assert(whammy_deadzone < whammy_max - whammy_min);
}
