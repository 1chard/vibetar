#include "boot_gesture.hpp"
using namespace guitar;
namespace {
constexpr bool plain_mode_test() {
    BootGesture gesture{3000};
    if (gesture.update(true, false, false, 0) != ModeAction::ToggleUsb) return false;
    if (gesture.update(true, false, false, 10000) != ModeAction::None) return false;
    if (gesture.update(false, false, false, 10001) != ModeAction::None) return false;
    return gesture.update(true, false, false, 10002) == ModeAction::ToggleUsb;
}
constexpr bool chord_test() {
    BootGesture gesture{3000};
    if (gesture.update(false, true, true, 1) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 10) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 3009) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 3010) != ModeAction::EnterBootsel) return false;
    return gesture.update(true, true, true, 10000) == ModeAction::None;
}
constexpr bool cancelled_hold_test() {
    BootGesture gesture{3000};
    gesture.update(true, true, true, 0);
    if (gesture.update(true, true, false, 2999) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 3000) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 5999) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 6000) != ModeAction::EnterBootsel) return false;
    gesture.update(false, false, false, 6001);
    gesture.update(true, true, true, 6002);
    return gesture.update(true, true, true, 9002) == ModeAction::EnterBootsel;
}
constexpr bool wrap_test() {
    BootGesture gesture{3000};
    gesture.update(true, true, true, 0xfffffff0u);
    if (gesture.update(true, true, true, 2983) != ModeAction::None) return false;
    return gesture.update(true, true, true, 2984) == ModeAction::EnterBootsel;
}
constexpr bool incomplete_chord_test() {
    BootGesture gesture{3000};
    if (gesture.update(true, true, false, 1) != ModeAction::ToggleUsb) return false;
    if (gesture.update(true, true, false, 9999) != ModeAction::None) return false;
    if (gesture.update(true, true, true, 10000) != ModeAction::None) return false;
    return gesture.update(true, true, true, 13000) == ModeAction::EnterBootsel;
}
static_assert(plain_mode_test(), "MODE still toggles once, even when held");
static_assert(chord_test(), "BOOTSEL chord requires a continuous 3-second hold");
static_assert(cancelled_hold_test(), "Released buttons cancel/rearm BOOTSEL hold");
static_assert(wrap_test(), "BOOTSEL timer handles uint32 wrap");
static_assert(incomplete_chord_test(), "Partial chords cannot trigger BOOTSEL");
}
