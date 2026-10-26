#pragma once

#include <cstddef>
#include <array>
#include <cstdint>

namespace guitar {
enum class UsbMode : uint8_t { Hid, XInput, RockBand }; // XInput retains Guitar Hero identity
constexpr UsbMode next_mode(UsbMode mode) {
    switch (mode) {
    case UsbMode::Hid: return UsbMode::XInput;
    case UsbMode::XInput: return UsbMode::RockBand;
    case UsbMode::RockBand: return UsbMode::Hid;
    }
    return UsbMode::Hid;
}
constexpr bool is_xinput_mode(UsbMode mode) {
    return mode == UsbMode::XInput || mode == UsbMode::RockBand;
}
enum class Button : uint8_t {
    Green, Red, Yellow, Blue, Orange, StrumUp, StrumDown,
    Start, Back, Tilt, Guide, Count
};
constexpr uint16_t bit(Button button) { return uint16_t{1} << static_cast<uint8_t>(button); }
struct State {
    uint16_t buttons = 0;
    uint16_t whammy = 0; // normalized 0..65535, resting at zero
    bool left = false, right = false;
    uint8_t solo_frets = 0, pickup = 0;
    constexpr bool pressed(Button button) const { return (buttons & bit(button)) != 0; }
};

// Packed wire payloads; RP2040 is little endian. TinyUSB prepends HID report ID 1.
struct __attribute__((packed)) HidReport {
    int8_t x = 0, y = 0, z = -127, rz = 0, rx = 0, ry = 0;
    uint8_t hat = 0;
    uint32_t buttons = 0;
};
struct __attribute__((packed)) XInputReport {
    uint8_t type = 0, length = 20;
    uint16_t buttons = 0;
    uint8_t left_trigger = 0, right_trigger = 0;
    int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;
    uint8_t reserved[6] = {};
};
static_assert(sizeof(HidReport) == 11 && offsetof(HidReport, buttons) == 7);
static_assert(sizeof(XInputReport) == 20 && offsetof(XInputReport, right_x) == 10);

constexpr uint16_t normalize_whammy(uint16_t raw, uint16_t min, uint16_t max,
                                   uint16_t deadzone, bool inverted) {
    if (min >= max || deadzone >= max - min) return 0;
    const uint16_t clamped = raw < min ? min : (raw > max ? max : raw);
    const uint16_t value = inverted ? max - clamped : clamped - min;
    if (value <= deadzone) return 0;
    return static_cast<uint16_t>(uint32_t(value - deadzone) * 65535u / (max - min - deadzone));
}

constexpr HidReport make_hid_report(const State& state) {
    HidReport report{};
    // HID buttons 1..5 = frets, 6 = start, 7 = back, 8 = tilt, 9 = guide.
    for (uint8_t i = 0; i < 5; ++i)
        if ((state.buttons | state.solo_frets) & (1u << i)) report.buttons |= 1u << i;
    report.buttons |= uint32_t(state.solo_frets & 31) << 10; // buttons 11..15: dedicated solo frets
    if (state.solo_frets) report.buttons |= 1u << 9;
    constexpr Button menu[] = {Button::Start, Button::Back, Button::Tilt, Button::Guide};
    for (uint8_t i = 0; i < 4; ++i)
        if (state.pressed(menu[i])) report.buttons |= 1u << (5 + i);
    const bool up = state.pressed(Button::StrumUp), down = state.pressed(Button::StrumDown);
    const int vertical = int(down) - int(up), horizontal = int(state.right) - int(state.left);
    report.hat = vertical < 0 ? (horizontal < 0 ? 8 : horizontal > 0 ? 2 : 1) :
                 vertical > 0 ? (horizontal < 0 ? 6 : horizontal > 0 ? 4 : 5) :
                 horizontal < 0 ? 7 : horizontal > 0 ? 3 : 0;
    report.z = static_cast<int8_t>(int32_t(state.whammy) * 254 / 65535 - 127);
    return report;
}

constexpr XInputReport make_xinput_report(const State& state, UsbMode mode = UsbMode::XInput) {
    XInputReport report{};
    // Xbox layout: green=A, red=B, yellow=Y, blue=X, orange=LB.
    constexpr uint16_t mapping[] = {0x1000, 0x2000, 0x8000, 0x4000, 0x0100,
                                    0, 0, 0x0010, 0x0020, 0, 0x0400};
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::Count); ++i)
        if ((state.buttons | state.solo_frets) & (1u << i)) report.buttons |= mapping[i];
    const bool up = state.pressed(Button::StrumUp), down = state.pressed(Button::StrumDown);
    if (up != down) report.buttons |= up ? 0x0001 : 0x0002;
    if (state.left != state.right) report.buttons |= state.left ? 0x0004 : 0x0008;
    if (mode == UsbMode::RockBand) {
        if (state.solo_frets) report.buttons |= 0x0040; // LSB marks every fret in this report as solo
        report.left_trigger = state.pickup;
    }
    // Guitar Hero/Xplorer and Rock Band: normal frets and axes share this layout.
    // Whammy uses the full signed range, resting at -32768.
    // The existing digital Tilt input synthesizes a vertical-orientation axis.
    report.right_x = static_cast<int16_t>(int32_t(state.whammy) - 32768);
    report.right_y = state.pressed(Button::Tilt) ? 32767 : 0;
    return report;
}
constexpr uint8_t resolve_solo(uint16_t normal, bool modifier, uint8_t dedicated) {
    return (dedicated | (modifier ? normal : 0)) & 31;
}
constexpr uint8_t pickup_position(uint16_t raw, uint16_t min, uint16_t max, bool inverted) {
    const auto value = normalize_whammy(raw, min, max, 0, inverted);
    constexpr uint8_t positions[] = {0x17, 0x4b, 0x79, 0xab, 0xe0};
    return positions[uint32_t(value) * 5 / 65536];
}
// References are in physical position order; they need not be evenly spaced or sorted.
constexpr uint8_t pickup_position(uint16_t raw, const std::array<uint16_t,5>& references, bool inverted) {
    constexpr uint8_t positions[] = {0x17,0x4b,0x79,0xab,0xe0};
    size_t nearest=0;
    uint32_t distance=65536;
    for(size_t i=0;i<references.size();++i) {
        const uint32_t delta=raw>references[i] ? raw-references[i] : references[i]-raw;
        // Equal distances keep the first position for deterministic boundaries.
        if(delta<distance) {nearest=i;distance=delta;}
    }
    return positions[inverted ? 4-nearest : nearest];
}
}
