#include "gpio_report.hpp"
using namespace guitar;
constexpr uint32_t mask(const GpioPayload& data, size_t offset) {
    return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
           uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
}
constexpr bool gpio_test() {
    RuntimeConfig config;
    config.button_pins[0] = 22;
    config.whammy_enabled = true;
    const auto data = encode_gpio_report(config, {bit(Button::Green), 65535}, true, 4095);
    if (mask(data, 8) != ((1u << 22) | (1u << 13)) || data[6] != 1 || data[7]) return false;
    if (!(mask(data, 12) & (1u << 26)) || (mask(data, 12) & (1u << 2))) return false;
    if (data[16] != 255 || data[17] != 15 || data[18] != 255 || data[19] != 255) return false;
    config.whammy_enabled = false;
    const auto idle = encode_gpio_report(config, {}, false, 4095);
    return !mask(idle, 8) && !(mask(idle, 12) & (1u << 26)) && !idle[16] && !idle[17];
}
static_assert(gpio_test(), "GPIO snapshot respects saved pin mapping, MODE, ADC and endian layout");
constexpr bool full_scan_test() {
    RuntimeConfig config;
    // GP0 is unassigned yet must be visible, GP22 high stays inactive.
    const auto data = encode_gpio_scan(config, {}, false, digital_scan_mask & ~1u, {0, 2048, 4095});
    if (data[4] != 2 || data[8] != 1 || data[9] || data[10] || data[11]) return false;
    if (data[12] & 1u) return false; // never implicitly assign the discovered pin
    if (data[20] != 0xff || data[21] != 0xff || data[22] != 0x7f || data[23]) return false;
    if (data[24] || data[25] || data[26] || data[27] != 8 || data[28] != 255 || data[29] != 15) return false;
    config.whammy_pin = 28;
    config.whammy_enabled = true;
    const auto selected = encode_gpio_scan(config, {0, 65535}, false, 0, {0, 2048, 4095});
    return selected[16] == 255 && selected[17] == 15 && selected[8] == 255 && selected[10] == 127;
}
static_assert(full_scan_test(), "Unassigned GPIO and all ADC channels must be scanned without changing assignments");
