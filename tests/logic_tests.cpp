// Compile-time regressions run in every firmware build; no hardware required.
#include "controller.hpp"
#include "debounce.hpp"

using namespace guitar;
namespace {
constexpr bool debounce_test() {
    DebouncedButton button{3};
    if (button.update(true, 10) || button.update(false, 11)) return false;
    if (button.update(true, 12) || button.update(true, 14)) return false;
    if (!button.update(true, 15) || !button.pressed()) return false;
    if (button.update(true, 500)) return false; // held button never retriggers
    if (button.update(false, 501) || button.update(false, 504) || button.pressed()) return false;
    if (button.update(true, 505)) return false;
    return button.update(true, 508);
}
constexpr bool rollover_test() {
    DebouncedButton button{3};
    if (button.update(true, 0xfffffffeu) || button.update(true, 0)) return false;
    return button.update(true, 1);
}
constexpr bool report_test() {
    constexpr auto idle_x = make_xinput_report({});
    constexpr auto idle_h = make_hid_report({});
    if (idle_x.type != 0 || idle_x.length != 20 || idle_x.buttons || idle_x.right_x != -32768 || idle_x.right_y) return false;
    if (idle_h.buttons || idle_h.hat || idle_h.z != -127) return false;
    constexpr uint16_t frets = bit(Button::Green) | bit(Button::Red) | bit(Button::Yellow) |
                               bit(Button::Blue) | bit(Button::Orange);
    if (make_xinput_report({frets, 0}).buttons != 0xf100) return false;
    if (make_hid_report({frets, 0}).buttons != 0x1f) return false;
    constexpr uint16_t menu = bit(Button::Start) | bit(Button::Back) | bit(Button::Tilt) | bit(Button::Guide);
    if (make_xinput_report({menu, 0}).buttons != 0x0430 || make_xinput_report({menu, 0}).right_y != 32767) return false;
    if (make_hid_report({menu, 0}).buttons != 0x1e0) return false;
    if (make_hid_report({bit(Button::StrumUp), 0}).hat != 1 ||
        make_hid_report({bit(Button::StrumDown), 0}).hat != 5) return false;
    if (make_xinput_report({bit(Button::StrumUp), 0}).buttons != 1 ||
        make_xinput_report({bit(Button::StrumDown), 0}).buttons != 2) return false;
    constexpr uint16_t both = bit(Button::StrumUp) | bit(Button::StrumDown);
    if (make_hid_report({both, 0}).hat || make_xinput_report({both, 0}).buttons) return false;
    return make_hid_report({0, 65535}).z == 127 && make_xinput_report({0, 65535}).right_x == 32767;
}
static_assert(debounce_test(), "Debounce / mode button press regression");
static_assert(rollover_test(), "32-bit millisecond wrap regression");
static_assert(report_test(), "HID / XInput mapping regression");
static_assert(next_mode(UsbMode::Hid) == UsbMode::XInput);
static_assert(next_mode(UsbMode::XInput) == UsbMode::RockBand);
static_assert(next_mode(UsbMode::RockBand) == UsbMode::Hid);
static_assert(next_mode(next_mode(next_mode(UsbMode::Hid))) == UsbMode::Hid);
static_assert(normalize_whammy(100, 100, 4000, 32, false) == 0);
static_assert(normalize_whammy(132, 100, 4000, 32, false) == 0);
static_assert(normalize_whammy(4095, 100, 4000, 32, false) == 65535);
static_assert(normalize_whammy(0, 100, 4000, 32, true) == 65535);
static_assert(normalize_whammy(4000, 100, 4000, 32, true) == 0);
static_assert(normalize_whammy(2000, 100, 100, 0, false) == 0);
}
