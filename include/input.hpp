#pragma once
#include "config.hpp"
#include "controller.hpp"
#include "debounce.hpp"
#include "boot_gesture.hpp"
#include "gpio_report.hpp"
#include "runtime_config.hpp"
#include "adxl345.hpp"
#include <array>

namespace guitar {
class Input {
public:
    void init(const RuntimeConfig& config);
    void reconfigure(const RuntimeConfig& config);
    void sample(uint32_t now);
    const State& state() const { return state_; }
    GpioPayload gpio_report() const { return encode_gpio_report(config_, digital_state(), mode_.pressed(), raw_adc_); }
    GpioScanPayload gpio_scan() const { return encode_gpio_scan(config_, digital_state(), mode_.pressed(), raw_levels_, adc_samples_); }
    bool take_mode_press();
    bool take_bootsel_request();
    bool recovery_chord() const { return state_.pressed(Button::Start) && state_.pressed(Button::Back) && !mode_.pressed(); }
    std::array<uint8_t,20> tilt_report() const {
        std::array<uint8_t,20> p{'X','C','T','L',1,static_cast<uint8_t>(config_.tilt.source),
                               static_cast<uint8_t>(tilt_valid_),static_cast<uint8_t>(state_.pressed(Button::Tilt))};
        const auto axes = accelerometer_.axes();
        const int16_t values[] = {tilt_value_,axes[0],axes[1],axes[2]};
        for (size_t i = 0; i < 4; ++i) { p[8+i*2] = static_cast<uint8_t>(values[i]); p[9+i*2] = static_cast<uint8_t>(values[i] >> 8); }
        return p;
    }
private:
    State digital_state() const {
        auto digital = state_;
        // GPIO reports describe physical buttons; sensor Tilt lives in feature 11.
        digital.buttons &= ~bit(Button::Tilt);
        if (buttons_[static_cast<size_t>(Button::Tilt)].pressed()) digital.buttons |= bit(Button::Tilt);
        return digital;
    }
    std::array<DebouncedButton, static_cast<size_t>(Button::Count)> buttons_{};
    DebouncedButton mode_{config::debounce_ms};
    std::array<DebouncedButton, 8> extras_buttons_{};
    Adxl345 accelerometer_{};
    int16_t tilt_value_ = 0;
    bool tilt_valid_ = false, tilt_pressed_ = false;
    State state_{};
    bool mode_press_ = false;
    bool bootsel_request_ = false;
    BootGesture boot_gesture_{config::bootsel_hold_ms};
    RuntimeConfig config_{};
    uint16_t raw_adc_ = 0;
    uint32_t raw_levels_ = digital_scan_mask;
    std::array<uint16_t, 3> adc_samples_{};
};
static_assert(config::button_pins.size() == static_cast<size_t>(Button::Count));
}
