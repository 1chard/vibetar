#pragma once
#include <cstdint>
namespace guitar::bluetooth {
enum class Phase : uint8_t { Idle, Searching, Pairing, Connected, Waiting };
// An open pairing window alone is not an active connection attempt.
constexpr Phase phase(bool enabled,bool working,bool searching,bool attempting,bool connected,bool usable) {
    if(!enabled || !working)return Phase::Idle;
    if(connected && usable)return Phase::Connected;
    if(attempting || connected)return Phase::Pairing;
    return searching?Phase::Searching:Phase::Waiting;
}
Phase connection_phase();
}
