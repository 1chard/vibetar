#include "controller.hpp"
#include "xinput_profile.hpp"
using namespace guitar;
namespace {
constexpr bool guitar_report_test() {
    if (make_xinput_report({bit(Button::Green), 0}).buttons != 0x1000 ||
        make_xinput_report({bit(Button::Red), 0}).buttons != 0x2000 ||
        make_xinput_report({bit(Button::Yellow), 0}).buttons != 0x8000 ||
        make_xinput_report({bit(Button::Blue), 0}).buttons != 0x4000 ||
        make_xinput_report({bit(Button::Orange), 0}).buttons != 0x0100) return false;
    if (make_xinput_report({0, 0}).right_x != -32768 ||
        make_xinput_report({0, 32768}).right_x != 0 ||
        make_xinput_report({0, 65535}).right_x != 32767) return false;
    const auto tilt = make_xinput_report({bit(Button::Tilt), 0});
    if (tilt.buttons != 0 || tilt.right_y != 32767 || tilt.right_x != -32768) return false;
    const auto neutral = make_xinput_report({});
    if (neutral.left_trigger || neutral.right_trigger || neutral.left_x || neutral.left_y) return false;
    for (auto byte : neutral.reserved) if (byte) return false;
    const uint16_t all_buttons = (1u << static_cast<uint8_t>(Button::Count)) - 1;
    const auto all = make_xinput_report({static_cast<uint16_t>(all_buttons & ~bit(Button::StrumDown)), 65535});
    return (all.buttons & ~xinput::supported_buttons) == 0;
}
constexpr bool control_test() {
    using namespace xinput;
    if (control_reply(0xc1, 1, 0x0100, 0) != ControlReply::InputCapabilities ||
        control_reply(0xc1, 1, 0, 0) != ControlReply::VibrationCapabilities ||
        control_reply(0xc0, os_vendor_code, 0, 4) != ControlReply::CompatibleId) return false;
    return control_reply(0x41, 1, 0x0100, 0) == ControlReply::Unsupported &&
           control_reply(0xc1, 1, 0x0100, 1) == ControlReply::Unsupported &&
           control_reply(0xc1, 1, 0x0200, 0) == ControlReply::Unsupported &&
           control_reply(0xc0, os_vendor_code, 0, 5) == ControlReply::Unsupported;
}
static_assert(guitar_report_test(), "Guitar Hero report regression");
static_assert(control_test(), "XUSB vendor request routing regression");
static_assert(xinput::subtype == 7 && xinput::vendor_id == 0x1430 && xinput::product_id == 0x4748);
static_assert(xinput::input_capabilities.size() == 20 && xinput::input_capabilities[1] == 20);
static_assert(xinput::vibration_capabilities.size() == 8 && xinput::vibration_capabilities[3] == 0);
static_assert(xinput::compatible_id.size() == 40 && xinput::compatible_id[18] == 'X');
static_assert(offsetof(XInputReport, right_y) == 12);
static_assert(xinput::profile_for(UsbMode::RockBand).subtype == 6);
static_assert(xinput::profile_for(UsbMode::XInput).subtype == 7);
static_assert(xinput::rock_band.vendor_id == 0x1bad && xinput::rock_band.product_id == 0x0002);
static_assert(is_xinput_mode(UsbMode::RockBand) && is_xinput_mode(UsbMode::XInput) && !is_xinput_mode(UsbMode::Hid));
constexpr bool rock_band_report_test() {
    // With no optional inputs configured, the original report remains unchanged.
    const auto report = make_xinput_report({bit(Button::Orange) | bit(Button::Tilt), 65535});
    const auto idle = make_xinput_report({});
    return report.buttons == 0x100 && report.right_y == 32767 && report.right_x == 32767 &&
           idle.right_x == -32768 && !idle.right_y && !report.left_trigger &&
           !report.left_x && !report.left_y && !(report.buttons & 0x40) &&
           xinput::rock_band.input_capabilities[2] == uint8_t(xinput::supported_buttons | 0x40) &&
           xinput::rock_band.input_capabilities[3] == uint8_t(xinput::supported_buttons >> 8);
}
static_assert(rock_band_report_test(), "Rock Band standard frets/axes/capabilities regression");
}
