#pragma once
#include <cstdint>
namespace guitar::bluetooth {
// IRQ timestamps can be newer than the caller's main-loop snapshot.
constexpr bool elapsed_at_least(uint32_t now,uint32_t started,uint32_t duration){
    const int32_t delta=static_cast<int32_t>(now-started);
    return delta>=0 && static_cast<uint32_t>(delta)>=duration;
}
// GATT discovery must wait for the Security Manager's completion event.
struct SecurityGate {
    bool encrypted=false,complete=false;
    constexpr void reset(){encrypted=false;complete=false;}
    constexpr void encryption(bool enabled){encrypted=enabled;if(!enabled)complete=false;}
    constexpr void finished(bool success){complete=success;if(success)encrypted=true;}
    constexpr bool ready()const{return encrypted && complete;}
};
}
