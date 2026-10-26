#pragma once
#include "config.hpp"
#include "tilt_config.hpp"
#include "host_policy.hpp"
#include "output_config.hpp"
#include "battery_config.hpp"
#include "bluetooth_config.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace guitar {
inline constexpr uint8_t gamepad_report_id = 1;
inline constexpr uint8_t config_report_id = 2;
inline constexpr uint8_t defaults_report_id = 3;
inline constexpr size_t config_payload_size = 24;
using ConfigPayload = std::array<uint8_t, config_payload_size>;
enum class ConfigStatus : uint8_t { Ready = 0, Pending = 1, Invalid = 2, FlashError = 3 };
inline constexpr uint8_t extras_report_id = 7, extras_defaults_report_id = 8;
inline constexpr uint8_t unused_pin = 255;
using ExtrasPayload = std::array<uint8_t, 24>;
struct ExtrasConfig {
    // left, right, solo modifier, solo green/red/yellow/blue/orange
    std::array<uint8_t, 8> digital_pins = {255,255,255,255,255,255,255,255};
    uint8_t pickup_pin = unused_pin;
    bool pickup_inverted = false;
    uint16_t pickup_min = 0, pickup_max = 4095;
    bool pickup_calibrated = false;
    std::array<uint16_t, 5> pickup_values = {0,1024,2048,3071,4095};
};

struct RuntimeConfig {
    BluetoothConfig bluetooth{};
    BatteryConfig battery{};
    OutputsConfig outputs{};
    HostConfig host{};
    TiltConfig tilt{};
    ExtrasConfig extras{};
    std::array<uint8_t, 11> button_pins = config::button_pins;
    uint8_t mode_pin = config::mode_pin;
    uint8_t whammy_pin = config::whammy_pin;
    bool whammy_enabled = config::whammy_enabled;
    bool whammy_inverted = config::whammy_inverted;
    uint16_t whammy_min = config::whammy_min;
    uint16_t whammy_max = config::whammy_max;
    uint16_t whammy_deadzone = config::whammy_deadzone;
};
constexpr bool valid_config(const RuntimeConfig& c) {
    if(!valid_battery(c.battery))return false;
    if(!valid_outputs(c.outputs)) return false;
    for(const auto& r:c.outputs.rules) if(r.pin!=unused_pin) {
        if(r.pin==c.mode_pin || (c.tilt.source==TiltSource::Adxl345 && (r.pin==c.tilt.sda || r.pin==c.tilt.scl))) return false;
        for(auto pin:c.button_pins) if(r.pin==pin) return false;
        for(auto pin:c.extras.digital_pins) if(r.pin==pin) return false;
    }
    if(static_cast<uint8_t>(c.host.policy)>2) return false;
    if (!valid_tilt(c.tilt)) return false;
    if (c.tilt.source == TiltSource::Adc &&
        ((c.whammy_enabled && c.tilt.adc_pin == c.whammy_pin) || c.tilt.adc_pin == c.extras.pickup_pin)) return false;
    if (c.tilt.source == TiltSource::Adxl345) {
        for (auto pin : {c.tilt.sda, c.tilt.scl}) {
            if (pin == c.mode_pin) return false;
            for (auto base : c.button_pins) if (pin == base) return false;
            for (auto extra : c.extras.digital_pins) if (pin == extra) return false;
        }
    }
    for (size_t i = 0; i < c.extras.digital_pins.size(); ++i) {
        const auto pin = c.extras.digital_pins[i];
        if (pin == unused_pin) continue;
        if (pin > 22 || pin == c.mode_pin) return false;
        for (auto base : c.button_pins) if (pin == base) return false;
        for (size_t j = i + 1; j < c.extras.digital_pins.size(); ++j)
            if (pin == c.extras.digital_pins[j]) return false;
    }
    const auto& e = c.extras;
    if (e.pickup_calibrated) for (size_t i=0;i<5;++i) {
        if (e.pickup_values[i]>4095) return false;
        for (size_t j=i+1;j<5;++j) if(e.pickup_values[i]==e.pickup_values[j]) return false;
    }
    if ((e.pickup_pin != unused_pin && (e.pickup_pin < 26 || e.pickup_pin > 28 ||
         (c.whammy_enabled && e.pickup_pin == c.whammy_pin))) ||
        e.pickup_min >= e.pickup_max || e.pickup_max > 4095) return false;
    // GP23..25/29 belong to Pico W onboard circuitry; GP26..28 reserved for ADC.
    if (c.mode_pin > 22 || c.whammy_pin < 26 || c.whammy_pin > 28) return false;
    for (size_t i = 0; i < c.button_pins.size(); ++i) {
        // Tilt can be disconnected when using an ADC/accelerometer (or disabled).
        if (i == 9 && c.button_pins[i] == unused_pin) continue;
        if (c.button_pins[i] > 22 || c.button_pins[i] == c.mode_pin) return false;
        for (size_t j = i + 1; j < c.button_pins.size(); ++j)
            if (c.button_pins[i] == c.button_pins[j]) return false;
    }
    return c.whammy_min < c.whammy_max && c.whammy_max <= 4095 &&
           c.whammy_deadzone < c.whammy_max - c.whammy_min;
}
constexpr ConfigPayload encode_config(const RuntimeConfig& c, ConfigStatus status = ConfigStatus::Ready) {
    ConfigPayload p{};
    p[0] = 1; // protocol version
    p[1] = static_cast<uint8_t>(status);
    for (size_t i = 0; i < c.button_pins.size(); ++i) p[i + 2] = c.button_pins[i];
    p[13] = c.mode_pin;
    p[14] = c.whammy_pin;
    p[15] = (c.whammy_enabled ? 1 : 0) | (c.whammy_inverted ? 2 : 0);
    const uint16_t values[] = {c.whammy_min, c.whammy_max, c.whammy_deadzone};
    for (size_t i = 0; i < 3; ++i) {
        p[16 + i * 2] = static_cast<uint8_t>(values[i]);
        p[17 + i * 2] = static_cast<uint8_t>(values[i] >> 8);
    }
    return p;
}
constexpr bool decode_config(const ConfigPayload& p, RuntimeConfig& c) {
    if (p[0] != 1 || p[1] != 0 || (p[15] & ~3u) || p[22] || p[23]) return false;
    RuntimeConfig candidate = c;
    for (size_t i = 0; i < candidate.button_pins.size(); ++i) candidate.button_pins[i] = p[i + 2];
    candidate.mode_pin = p[13];
    candidate.whammy_pin = p[14];
    candidate.whammy_enabled = (p[15] & 1) != 0;
    candidate.whammy_inverted = (p[15] & 2) != 0;
    candidate.whammy_min = p[16] | uint16_t(p[17]) << 8;
    candidate.whammy_max = p[18] | uint16_t(p[19]) << 8;
    candidate.whammy_deadzone = p[20] | uint16_t(p[21]) << 8;
    if (!valid_config(candidate)) return false;
    c = candidate;
    return true;
}
constexpr bool same_config(const RuntimeConfig& a, const RuntimeConfig& b) {
    if(!same_bluetooth(a.bluetooth,b.bluetooth))return false;
    if(!same_battery(a.battery,b.battery))return false;
    if(!same_outputs(a.outputs,b.outputs)) return false;
    if(a.host.policy!=b.host.policy || a.host.boot_rock_band!=b.host.boot_rock_band) return false;
    if (!same_tilt(a.tilt, b.tilt)) return false;
    for (size_t i = 0; i < a.extras.digital_pins.size(); ++i)
        if (a.extras.digital_pins[i] != b.extras.digital_pins[i]) return false;
    if (a.extras.pickup_pin != b.extras.pickup_pin || a.extras.pickup_inverted != b.extras.pickup_inverted ||
        a.extras.pickup_calibrated != b.extras.pickup_calibrated) return false;
    if (a.extras.pickup_calibrated) {
        for(size_t i=0;i<5;++i) if (a.extras.pickup_values[i] != b.extras.pickup_values[i]) return false;
    } else if (a.extras.pickup_min != b.extras.pickup_min || a.extras.pickup_max != b.extras.pickup_max) return false;
    const auto pa = encode_config(a), pb = encode_config(b);
    for (size_t i = 0; i < pa.size(); ++i) if (pa[i] != pb[i]) return false;
    return true;
}
static_assert(valid_config(RuntimeConfig{}));
constexpr ExtrasPayload encode_extras(const RuntimeConfig& c, ConfigStatus status = ConfigStatus::Ready) {
    ExtrasPayload p{1, static_cast<uint8_t>(status)};
    for (size_t i = 0; i < 8; ++i) p[2 + i] = c.extras.digital_pins[i];
    p[10] = c.extras.pickup_pin;
    p[11] = c.extras.pickup_inverted;
    p[12] = static_cast<uint8_t>(c.extras.pickup_min); p[13] = c.extras.pickup_min >> 8;
    p[14] = static_cast<uint8_t>(c.extras.pickup_max); p[15] = c.extras.pickup_max >> 8;
    if (c.extras.pickup_calibrated) {
        p[0]=2;
        for (size_t i=0;i<5;++i) {
            p[12+i*2]=static_cast<uint8_t>(c.extras.pickup_values[i]);
            p[13+i*2]=c.extras.pickup_values[i]>>8;
        }
    }
    return p;
}
constexpr bool decode_extras(const ExtrasPayload& p, RuntimeConfig& c) {
    if ((p[0] != 1 && p[0] != 2) || p[1] != 0 || p[11] > 1) return false;
    for (size_t i = p[0]==2 ? 22 : 16; i < p.size(); ++i) if (p[i]) return false;
    auto candidate = c;
    for (size_t i = 0; i < 8; ++i) candidate.extras.digital_pins[i] = p[2 + i];
    candidate.extras.pickup_pin = p[10]; candidate.extras.pickup_inverted = p[11];
    candidate.extras.pickup_min = p[12] | uint16_t(p[13]) << 8;
    candidate.extras.pickup_max = p[14] | uint16_t(p[15]) << 8;
    candidate.extras.pickup_calibrated=p[0]==2;
    if (candidate.extras.pickup_calibrated) {
        candidate.extras.pickup_min=0; candidate.extras.pickup_max=4095;
        for(size_t i=0;i<5;++i) candidate.extras.pickup_values[i]=p[12+i*2] | uint16_t(p[13+i*2])<<8;
    }
    if (!valid_config(candidate)) return false;
    c = candidate; return true;
}
}
