#include "xsm3_protocol.hpp"
#include "host_policy.hpp"
namespace {
using namespace guitar::auth;
constexpr bool checks(){
    if(command(0xc1,0x81,0x101,29)!=Command::Identify || command(0x41,0x82,0x101,34)!=Command::Init ||
       command(0xc1,0x83,1,46)!=Command::Response || command(0xc1,0x86,1,2)!=Command::Status ||
       command(0x41,0x87,1,22)!=Command::Verify || command(0x41,0x84,1,0)!=Command::Acknowledge)return false;
    if(command(0xc1,0x81,0x201,29)!=Command::None || command(0xc1,0x81,0,29)!=Command::None ||
       command(0x41,0x82,1,35)!=Command::None || command(0xc1,0x82,1,34)!=Command::None)return false;
    uint8_t init[34]{9,0x40,0,0,28};
    if(!valid_packet(init,34,Command::Init) || valid_packet(init,33,Command::Init))return false;
    init[5]=1;if(valid_packet(init,34,Command::Init))return false;
    init[33]=1;if(!valid_packet(init,34,Command::Init))return false;
    Session s{};
    if(s.begin(Command::Verify) || s.begin(Command::Init))return false;
    s.phase=Phase::Identified;if(!s.begin(Command::Init) || s.status()!=1 || s.begin(Command::Verify))return false;
    s.finish(true);if(s.status()!=2 || !s.begin(Command::Verify))return false;
    s.finish(true);if(!s.begin(Command::Verify))return false; // multiple verify rounds
    s.finish(false);if(s.ready() || s.status()!=0 || s.begin(Command::Verify))return false;
    s.reset();if(s.phase!=Phase::Idle)return false;
    guitar::HostDetector host{};host.request(0xc1,0x81,0x5b17,0x101,29);
    return host.kind()==guitar::HostKind::Xbox360;
}
static_assert(checks(),"XSM3 request routing, packet bounds/checksum, session order, failure/reset and retail host detection");
}
