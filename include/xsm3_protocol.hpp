#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace guitar::auth {
inline constexpr uint8_t interface_id=1;
inline constexpr char security_string[]="Xbox Security Method 3, Version 1.00, \xa9 2005 Microsoft Corporation. All rights reserved.";
enum class Command:uint8_t {None,Identify=0x81,Init=0x82,Response=0x83,Acknowledge=0x84,Status=0x86,Verify=0x87};
constexpr Command command(uint8_t type,uint8_t request,uint16_t index,uint16_t length){
    // The high byte is a protocol selector: retail captures use 0x0103.
    if((index&255)!=interface_id || (index>>8)>1)return Command::None;
    if(type==0xc1 && length){
        if(request==0x81)return Command::Identify;
        if(request==0x83)return Command::Response;
        if(request==0x86)return Command::Status;
    }
    if(type==0x41){
        if(request==0x82 && length==34)return Command::Init;
        if(request==0x87 && length==22)return Command::Verify;
        if(request==0x84 && !length)return Command::Acknowledge;
    }
    return Command::None;
}
constexpr bool valid_packet(const uint8_t* p,size_t length,Command cmd){
    const size_t expected=cmd==Command::Init?34:cmd==Command::Verify?22:0;
    if(!expected || length!=expected || p[0]!=9 || p[1]!=(cmd==Command::Init?0x40:0x41) || p[2] || p[3] || p[4]!=length-6)return false;
    uint8_t checksum=0;for(size_t i=5;i+1<length;++i)checksum^=p[i];
    return checksum==p[length-1];
}
enum class Phase:uint8_t {Idle,Identified,BusyInit,InitReady,BusyVerify,VerifyReady,Failed};
struct Session {
    Phase phase=Phase::Idle;
    constexpr void reset(){phase=Phase::Idle;}
    constexpr bool busy()const{return phase==Phase::BusyInit || phase==Phase::BusyVerify;}
    constexpr bool ready()const{return phase==Phase::InitReady || phase==Phase::VerifyReady;}
    constexpr bool begin(Command c){
        if(c==Command::Init && (phase==Phase::Identified || ready())){phase=Phase::BusyInit;return true;}
        if(c==Command::Verify && ready()){phase=Phase::BusyVerify;return true;}
        return false;
    }
    constexpr void finish(bool valid){phase=!valid?Phase::Failed:phase==Phase::BusyInit?Phase::InitReady:Phase::VerifyReady;}
    constexpr uint16_t status()const{return ready()?2:busy()?1:0;}
};
}
