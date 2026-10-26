#pragma once
#include "runtime_config.hpp"
#include "controller.hpp"
namespace guitar {
inline constexpr uint8_t gpio_report_id = 5;
inline constexpr uint8_t gpio_scan_report_id = 6;
inline constexpr uint32_t digital_scan_mask = 0x007fffff; // exposed GP0..GP22 only
using GpioPayload = std::array<uint8_t, 20>;
using GpioScanPayload = std::array<uint8_t, 30>;
constexpr GpioPayload encode_gpio_report(const RuntimeConfig& config, const State& state,
                                         bool mode, uint16_t raw_adc) {
    GpioPayload data{'X', 'C', 'G', 'P', 1, static_cast<uint8_t>(config.whammy_enabled)};
    data[6] = static_cast<uint8_t>(state.buttons);
    data[7] = static_cast<uint8_t>(state.buttons >> 8);
    uint32_t assigned = uint32_t{1} << config.mode_pin;
    uint32_t active = mode ? assigned : 0;
    for (size_t i = 0; i < config.button_pins.size(); ++i) {
        if (config.button_pins[i] == unused_pin) continue;
        const auto mask = uint32_t{1} << config.button_pins[i];
        assigned |= mask;
        if (state.buttons & (1u << i)) active |= mask;
    }
    if (config.whammy_enabled) assigned |= uint32_t{1} << config.whammy_pin;
    for (size_t i = 0; i < 4; ++i) {
        data[8 + i] = static_cast<uint8_t>(active >> (8 * i));
        data[12 + i] = static_cast<uint8_t>(assigned >> (8 * i));
    }
    const uint16_t raw = config.whammy_enabled ? raw_adc : 0;
    data[16] = static_cast<uint8_t>(raw);
    data[17] = static_cast<uint8_t>(raw >> 8);
    data[18] = static_cast<uint8_t>(state.whammy);
    data[19] = static_cast<uint8_t>(state.whammy >> 8);
    return data;
}
constexpr GpioScanPayload encode_gpio_scan(const RuntimeConfig& config, const State& state,
                                          bool mode, uint32_t levels,
                                          const std::array<uint16_t, 3>& adc) {
    const auto legacy = encode_gpio_report(config, state, mode, adc[config.whammy_pin - 26]);
    GpioScanPayload data{};
    for (size_t i = 0; i < legacy.size(); ++i) data[i] = legacy[i];
    data[4] = 2; // full-header scan, independent from configured button assignments
    const uint32_t active = ~levels & digital_scan_mask;
    uint32_t assigned = uint32_t(legacy[12]) | uint32_t(legacy[13]) << 8 |
                        uint32_t(legacy[14]) << 16 | uint32_t(legacy[15]) << 24;
    for (auto pin : config.extras.digital_pins) if (pin != unused_pin) assigned |= uint32_t{1} << pin;
    if (config.extras.pickup_pin != unused_pin) assigned |= uint32_t{1} << config.extras.pickup_pin;
    if (config.tilt.source == TiltSource::Adc) assigned |= uint32_t{1} << config.tilt.adc_pin;
    if (config.tilt.source == TiltSource::Adxl345) assigned |= (uint32_t{1} << config.tilt.sda) | (uint32_t{1} << config.tilt.scl);
    for(const auto& r:config.outputs.rules) if(r.pin<=22) assigned |= uint32_t{1}<<r.pin;
    for (size_t i = 0; i < 4; ++i) {
        data[8 + i] = static_cast<uint8_t>(active >> (8 * i));
        data[12 + i] = static_cast<uint8_t>(assigned >> (8 * i));
        data[20 + i] = static_cast<uint8_t>(digital_scan_mask >> (8 * i));
    }
    for (size_t i = 0; i < adc.size(); ++i) {
        data[24 + 2 * i] = static_cast<uint8_t>(adc[i]);
        data[25 + 2 * i] = static_cast<uint8_t>(adc[i] >> 8);
    }
    return data;
}
}
