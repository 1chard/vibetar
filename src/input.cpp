#include "input.hpp"
#include "hardware/adc.h"
#include "hardware/gpio.h"

namespace guitar {
void Input::init(const RuntimeConfig& settings) {
    accelerometer_.stop(); // Restore old I2C pins before configuring the new mapping.
    config_ = settings;
    state_ = {};
    raw_adc_ = 0;
    raw_levels_ = digital_scan_mask;
    adc_samples_ = {};
    tilt_value_ = 0; tilt_valid_ = tilt_pressed_ = false;
    mode_press_ = false;
    bootsel_request_ = false;
    boot_gesture_ = BootGesture{config::bootsel_hold_ms};
    mode_ = DebouncedButton{config::debounce_ms};
    for (auto& button : extras_buttons_) button = DebouncedButton{config::debounce_ms};
    // Free header pins must have defined levels so a switch can be discovered
    // before assigning it. Do not touch onboard wireless/system GP23..25/29.
    for (uint8_t pin = 0; pin <= 22; ++pin) {
        if(output_owns(config_.outputs,pin)) continue;
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
    for (size_t i = 0; i < buttons_.size(); ++i) {
        buttons_[i] = DebouncedButton{config::debounce_ms};
    }
    adc_init();
    for (uint8_t pin = 26; pin <= 28; ++pin) adc_gpio_init(pin);
    accelerometer_.init(config_.tilt);
}
void Input::reconfigure(const RuntimeConfig& settings) {
    for (const auto pin : config_.button_pins) {
        if (pin == unused_pin) continue;
        gpio_disable_pulls(pin);
        gpio_deinit(pin);
    }
    gpio_disable_pulls(config_.mode_pin);
    gpio_deinit(config_.mode_pin);
    if (config_.whammy_enabled) gpio_deinit(config_.whammy_pin);
    init(settings);
}
void Input::sample(uint32_t now) {
    raw_levels_ = gpio_get_all() & digital_scan_mask;
    state_.buttons = 0;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const auto pin = config_.button_pins[i];
        buttons_[i].update(pin != unused_pin && !(raw_levels_ & (uint32_t{1} << pin)), now);
        if (buttons_[i].pressed()) state_.buttons |= uint16_t{1} << i;
    }
    mode_.update(!(raw_levels_ & (uint32_t{1} << config_.mode_pin)), now);
    uint8_t dedicated = 0;
    for (size_t i = 0; i < extras_buttons_.size(); ++i) {
        const auto pin = config_.extras.digital_pins[i];
        extras_buttons_[i].update(pin != unused_pin && !(raw_levels_ & (uint32_t{1} << pin)), now);
        if (i >= 3 && extras_buttons_[i].pressed()) dedicated |= uint8_t{1} << (i - 3);
    }
    state_.left = extras_buttons_[0].pressed(); state_.right = extras_buttons_[1].pressed();
    state_.solo_frets = resolve_solo(state_.buttons, extras_buttons_[2].pressed(), dedicated);
    const auto action = boot_gesture_.update(mode_.pressed(), state_.pressed(Button::Start),
                                             state_.pressed(Button::Back), now);
    mode_press_ |= action == ModeAction::ToggleUsb;
    bootsel_request_ |= action == ModeAction::EnterBootsel;
    // Three exposed ADCs, including unassigned channels. The game's whammy
    // still uses only its configured channel and calibration.
    for (uint8_t channel = 0; channel < adc_samples_.size(); ++channel) {
        adc_select_input(channel);
        adc_samples_[channel] = adc_read();
    }
    if (config_.whammy_enabled) {
        raw_adc_ = adc_samples_[config_.whammy_pin - 26];
        state_.whammy = normalize_whammy(raw_adc_, config_.whammy_min, config_.whammy_max,
                                         config_.whammy_deadzone, config_.whammy_inverted);
    }
    const auto& e = config_.extras;
    state_.pickup = e.pickup_pin == unused_pin ? 0 :
        e.pickup_calibrated ? pickup_position(adc_samples_[e.pickup_pin - 26], e.pickup_values, e.pickup_inverted) :
        pickup_position(adc_samples_[e.pickup_pin - 26], e.pickup_min, e.pickup_max, e.pickup_inverted);
    const auto& t = config_.tilt;
    if (t.source == TiltSource::Button) {
        tilt_valid_ = true; tilt_pressed_ = state_.pressed(Button::Tilt); tilt_value_ = tilt_pressed_ ? 1 : 0;
    } else {
        if (t.source == TiltSource::Adc) {
            tilt_valid_ = true; tilt_value_ = adc_samples_[t.adc_pin - 26];
        } else {
            accelerometer_.sample(now);
            tilt_valid_ = accelerometer_.valid(); tilt_value_ = accelerometer_.axes()[t.axis];
        }
        tilt_pressed_ = tilt_active(tilt_value_, t, tilt_pressed_, tilt_valid_);
        state_.buttons &= ~bit(Button::Tilt);
        if (tilt_pressed_) state_.buttons |= bit(Button::Tilt);
    }
}
bool Input::take_mode_press() {
    const bool result = mode_press_;
    mode_press_ = false;
    return result;
}
bool Input::take_bootsel_request() {
    const bool result = bootsel_request_;
    bootsel_request_ = false;
    return result;
}
}
