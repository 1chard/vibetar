#include "boot_gesture.hpp"
using namespace guitar;
constexpr bool gestures() {
 BootGesture g{3000};
 if(g.update(true,false,false,0)!=ModeAction::ToggleUsb)return false;
 if(g.update(true,false,false,5)!=ModeAction::None)return false;
 g.update(false,true,true,10,true);
 if(g.update(false,true,false,20,true)!=ModeAction::ToggleUsb)return false;
 g.update(false,true,true,100,true);
 if(g.update(false,true,true,3099,true)!=ModeAction::None)return false;
 if(g.update(false,true,true,3100,true)!=ModeAction::ForceHid)return false;
 if(g.update(false,true,true,4000,true)!=ModeAction::None)return false;
 if(g.update(false,false,false,4001,true)!=ModeAction::None)return false;
 g.update(false,true,true,5000);
 if(g.update(false,false,false,5100)!=ModeAction::None)return false;
 g.update(true,true,true,0xfffffff0u);
 return g.update(true,true,true,2984)==ModeAction::ForceHid;
}
static_assert(gestures(),"MODE, optional short chord, exclusive long HID recovery, no bootloader, wrap");
