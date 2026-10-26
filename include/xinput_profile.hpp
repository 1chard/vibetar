#pragma once
#include <array>
#include <cstdint>
#include "controller.hpp"

namespace guitar::xinput {
// Guitar Hero 2 Xplorer (wired Xbox 360 guitar), for PC XUSB driver binding.
inline constexpr uint16_t vendor_id = 0x1430;
inline constexpr uint16_t product_id = 0x4748;
inline constexpr uint16_t revision = 0x3122;
inline constexpr uint8_t subtype = 0x07; // XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE
inline constexpr uint16_t supported_buttons = 0xf53f; // frets, four D-pad directions, Start/Back, Guide
inline constexpr std::array<uint8_t, 20> input_capabilities = {
    0, 20,
    static_cast<uint8_t>(supported_buttons), static_cast<uint8_t>(supported_buttons >> 8),
    0, 0, // no auxiliary accelerometers on the trigger channels
    0, 0, 0, 0, // no touch strip / unused left stick Y
    0xff, 0xff, 0xff, 0xff, // whammy and digital tilt exposed as signed axes
    0, 0, 0, 0,
    0, 0 // no force feedback/audio/plugin modules advertised
};
inline constexpr std::array<uint8_t, 8> vibration_capabilities = {0, 8, 0, 0, 0, 0, 0, 0};
struct Profile {
    uint16_t vendor_id, product_id, revision;
    uint8_t subtype;
    std::array<uint8_t, 20> input_capabilities;
};
inline constexpr Profile guitar_hero = {vendor_id, product_id, revision, subtype, input_capabilities};
constexpr auto rock_band_capabilities() {
    auto caps = input_capabilities;
    caps[2] |= 0x40; // Solo fret modifier (LSB)
    caps[4] = 0xff; // Five-position pickup/effects selector on LT
    return caps;
}
inline constexpr Profile rock_band = {0x1bad, 0x0002, 0x0200, 0x06, rock_band_capabilities()};
constexpr const Profile& profile_for(UsbMode mode) {
    return mode == UsbMode::RockBand ? rock_band : guitar_hero;
}
inline constexpr uint8_t os_vendor_code = 0x20;
// Microsoft OS 1.0 Extended Compat ID: interface 0 binds to XUSB10.
inline constexpr std::array<uint8_t, 40> compatible_id = {
    40, 0, 0, 0, 0, 1, 4, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 'X', 'U', 'S', 'B', '1', '0', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
enum class ControlReply { Unsupported, InputCapabilities, VibrationCapabilities, CompatibleId };
constexpr ControlReply control_reply(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index) {
    // Vendor IN, interface recipient. TinyUSB routes these through the global
    // vendor callback before class-driver callbacks, even for interface requests.
    if (request_type == 0xc1 && request == 1 && index == 0) {
        if (value == 0x0100) return ControlReply::InputCapabilities;
        if (value == 0) return ControlReply::VibrationCapabilities;
    }
    if (request_type == 0xc0 && request == os_vendor_code && value == 0 && index == 4)
        return ControlReply::CompatibleId;
    return ControlReply::Unsupported;
}
}
