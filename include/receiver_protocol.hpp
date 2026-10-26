#pragma once
#include "controller.hpp"
#include <array>
#include <algorithm>
namespace guitar::wireless {
inline constexpr uint8_t state_report_id=26,feedback_report_id=27,role_report_id=28;
inline constexpr uint32_t stale_ms=250;
using Packet=std::array<uint8_t,16>;
using Feedback=std::array<uint8_t,8>;
constexpr Packet encode(const State& s,uint8_t mode_counter,uint16_t sequence){
    return {'X','C','R','S',1,static_cast<uint8_t>((s.left?1:0)|(s.right?2:0)),
        static_cast<uint8_t>(s.buttons),static_cast<uint8_t>(s.buttons>>8),
        static_cast<uint8_t>(s.whammy),static_cast<uint8_t>(s.whammy>>8),s.solo_frets,s.pickup,
        mode_counter,static_cast<uint8_t>(sequence),static_cast<uint8_t>(sequence>>8),0};
}
constexpr bool decode(const uint8_t* p,size_t length,State& s,uint8_t& counter,uint16_t& sequence){
    if(length!=16 || p[0]!='X' || p[1]!='C' || p[2]!='R' || p[3]!='S' || p[4]!=1 || (p[5]&~3) ||
       (p[7]&~7) || (p[10]&~31) || p[15])return false;
    if(p[11]!=0 && p[11]!=0x17 && p[11]!=0x4b && p[11]!=0x79 && p[11]!=0xab && p[11]!=0xe0)return false;
    s={static_cast<uint16_t>(p[6]|uint16_t(p[7])<<8),static_cast<uint16_t>(p[8]|uint16_t(p[9])<<8),
        bool(p[5]&1),bool(p[5]&2),p[10],p[11]};
    counter=p[12];sequence=p[13]|uint16_t(p[14])<<8;return true;
}
constexpr Feedback feedback(UsbMode mode,uint8_t mask){return {'X','C','R','F',1,static_cast<uint8_t>(mode),mask,0};}
constexpr bool decode_feedback(const uint8_t* p,size_t length,UsbMode& mode){
    if(length!=8 || p[0]!='X' || p[1]!='C' || p[2]!='R' || p[3]!='F' || p[4]!=1 || p[5]>2 ||
       (p[6]!=6 && p[6]!=7) || !(p[6]&(1<<p[5])) || p[7])return false;
    mode=static_cast<UsbMode>(p[5]);return true;
}
constexpr bool advertised(const uint8_t* p,size_t length){
    for(size_t offset=0;offset<length;){
        const size_t n=p[offset];if(!n || offset+n>=length)return false;
        if(n==8 && p[offset+1]==0xff && p[offset+2]==0xff && p[offset+3]==0xff &&
           p[offset+4]=='X' && p[offset+5]=='C' && p[offset+6]=='G' && p[offset+7]=='T' && p[offset+8]==1)return true;
        offset+=n+1;
    }
    return false;
}
// IRQ-owned link state; main loop takes a locked copy. Never return stale controls.
struct Link {
    State current{};
    uint32_t updated=0;
    uint16_t sequence=0;
    uint8_t counter=0,pending_mode=0;
    bool valid=false;
    constexpr void reset(){current={};updated=0;sequence=0;counter=0;pending_mode=0;valid=false;}
    constexpr bool fresh(uint32_t now)const{return valid && uint32_t(now-updated)<stale_ms;}
    constexpr bool accept(const uint8_t* p,size_t size,uint32_t now){
        State s{};uint8_t c=0;uint16_t seq=0;
        if(!decode(p,size,s,c,seq))return false;
        const bool continuous=fresh(now);
        if(continuous){const uint16_t delta=seq-sequence;if(!delta || delta>=0x8000)return false;}
        if(continuous){const uint8_t presses=c-counter;if(presses<=32)pending_mode=static_cast<uint8_t>(std::min<unsigned>(32,pending_mode+presses));}
        else pending_mode=0;
        current=s;counter=c;sequence=seq;updated=now;valid=true;return true;
    }
    constexpr State state(uint32_t now)const{return fresh(now)?current:State{};}
    constexpr bool take_mode(uint32_t now){if(!fresh(now)){pending_mode=0;return false;}if(!pending_mode)return false;--pending_mode;return true;}
};
}
