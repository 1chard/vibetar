#pragma once
#include <cstdint>
namespace guitar {
enum class ModeAction { None, ToggleUsb, ForceHid };
class BootGesture {
public:
 constexpr explicit BootGesture(uint32_t hold_ms):hold_ms_(hold_ms){}
 constexpr ModeAction update(bool mode,bool start,bool back,uint32_t now,bool tap_enabled=false) {
  const bool edge=mode && !previous_mode_;previous_mode_=mode;
  if(start && back) {
   if(!holding_){holding_=true;fired_=false;held_at_=now;}
   if(!fired_ && uint32_t(now-held_at_)>=hold_ms_){fired_=true;return ModeAction::ForceHid;}
   return ModeAction::None;
  }
  const bool tap=holding_ && !fired_ && uint32_t(now-held_at_)<hold_ms_ && tap_enabled;
  holding_=false;fired_=false;
  return tap || edge ? ModeAction::ToggleUsb : ModeAction::None;
 }
private:
 uint32_t hold_ms_,held_at_=0;
 bool previous_mode_=false,holding_=false,fired_=false;
};
}
