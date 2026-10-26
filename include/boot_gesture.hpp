#pragma once
#include <cstdint>

namespace guitar {
enum class ModeAction { None, ToggleUsb, EnterBootsel };

// Feed debounced inputs. A plain MODE press toggles USB; a deliberate chord
// enters the ROM bootloader. Incomplete holds are cancelled, not accumulated.
class BootGesture {
public:
    constexpr explicit BootGesture(uint32_t hold_ms) : hold_ms_(hold_ms) {}
    constexpr ModeAction update(bool mode, bool start, bool back, uint32_t now) {
        const bool mode_press = mode && !previous_mode_;
        previous_mode_ = mode;
        if (!(mode && start && back)) {
            holding_ = false;
            fired_ = false;
            return mode_press ? ModeAction::ToggleUsb : ModeAction::None;
        }
        if (!holding_) {
            holding_ = true;
            held_at_ = now;
        }
        if (!fired_ && uint32_t(now - held_at_) >= hold_ms_) {
            fired_ = true;
            return ModeAction::EnterBootsel;
        }
        return ModeAction::None;
    }
private:
    uint32_t hold_ms_, held_at_ = 0;
    bool previous_mode_ = false, holding_ = false, fired_ = false;
};
}
