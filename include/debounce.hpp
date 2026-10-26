#pragma once
#include <cstdint>

namespace guitar {
class DebouncedButton {
public:
    constexpr DebouncedButton() : interval_(3) {}
    constexpr explicit DebouncedButton(uint32_t interval) : interval_(interval) {}
    // Returns true once per debounced press. uint32 subtraction handles timer wrap.
    constexpr bool update(bool raw_pressed, uint32_t now) {
        if (raw_pressed != candidate_) {
            candidate_ = raw_pressed;
            changed_at_ = now;
        }
        if (candidate_ != stable_ && uint32_t(now - changed_at_) >= interval_) {
            stable_ = candidate_;
            return stable_;
        }
        return false;
    }
    constexpr bool pressed() const { return stable_; }
private:
    uint32_t interval_, changed_at_ = 0;
    bool candidate_ = false, stable_ = false;
};
}
