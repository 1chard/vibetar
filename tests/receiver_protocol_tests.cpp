#include "receiver_protocol.hpp"
#include "bluetooth_discovery.hpp"
using namespace guitar;
constexpr bool wireless_checks(){
    wireless::Link link{};
    State state{0x7ff,0xabcd,true,true,31,0xe0};
    auto p=wireless::encode(state,255,65535);
    if(!link.accept(p.data(),p.size(),100) || link.take_mode(100))return false;
    if(link.state(100).whammy!=0xabcd || link.state(100).solo_frets!=31)return false;
    if(link.accept(p.data(),p.size(),101))return false;
    p=wireless::encode(state,0,0);
    if(!link.accept(p.data(),p.size(),102) || !link.take_mode(102) || link.take_mode(102))return false;
    auto old=wireless::encode(state,1,65535);
    if(link.accept(old.data(),old.size(),103))return false;
    p[15]=1;if(link.accept(p.data(),p.size(),103))return false;
    if(link.state(352).buttons || link.take_mode(352))return false;
    p=wireless::encode(state,80,123);
    if(!link.accept(p.data(),p.size(),353) || link.take_mode(353))return false;
    link.reset();if(link.state(354).buttons)return false;
    UsbMode mode{};auto f=wireless::feedback(UsbMode::RockBand,6);
    if(!wireless::decode_feedback(f.data(),f.size(),mode) || mode!=UsbMode::RockBand)return false;
    f[5]=0;if(wireless::decode_feedback(f.data(),f.size(),mode))return false;
    uint8_t ad[]{2,1,6,8,255,255,255,'X','C','G','T',1};
    return wireless::advertised(ad,sizeof(ad)) && !wireless::advertised(ad,sizeof(ad)-1);
}
static_assert(wireless_checks(),"Full guitar state, sequence/counter wrap, replay, stale release, feedback and advertisement bounds");
constexpr bool discovery_checks(){
    bluetooth::Discovery scan{};uint8_t a[]{1,2,3,4,5,6},b[]{2,2,3,4,5,6};
    scan.observe(a,0,-60,100);scan.observe(a,0,-40,101);scan.observe(b,1,-80,102);
    auto report=scan.report(103,3,255);
    if(report[5]!=2 || report[15]!=static_cast<uint8_t>(-40) || !scan.contains(a,0,103) || scan.contains(a,1,103))return false;
    if(scan.contains(a,0,15101) || scan.report(15102,0,255)[5])return false;
    scan.clear();scan.observe(a,0,-60,0xfffffff0);
    if(!scan.contains(a,0,10))return false;
    bluetooth::ConnectPayload command{'X','C','C','N',1,0,0,0,1,2,3,4,5,6};
    if(!bluetooth::valid_connect(command.data(),16) || bluetooth::valid_connect(command.data(),15))return false;
    command[14]=1;return !bluetooth::valid_connect(command.data(),16);
}
static_assert(discovery_checks(),"Discovery deduplication, expiry/wrap, address type and malformed explicit connect commands");

#include "receiver_health.hpp"
constexpr bool security_order_checks(){
    guitar::bluetooth::SecurityGate gate{};
    gate.encryption(true);if(gate.ready())return false;
    gate.finished(true);if(!gate.ready())return false;
    gate.reset();gate.finished(true);if(!gate.ready())return false;
    gate.encryption(false);if(gate.ready())return false;
    gate.reset();gate.encryption(true);gate.finished(false);return !gate.ready();
}
static_assert(security_order_checks(),"Do not start discovery on encryption alone; support completed pairing/reencryption and reset/failure");
static_assert(!guitar::bluetooth::elapsed_at_least(100,101,2000),"A newer IRQ report must not look like a 49-day timeout");
static_assert(guitar::bluetooth::elapsed_at_least(2100,100,2000),"A genuinely stopped link must still disconnect");
static_assert(!guitar::bluetooth::elapsed_at_least(2100,101,2000),"Respect the actual timeout boundary");
static_assert(guitar::bluetooth::elapsed_at_least(10,0xfffffff0u,26),"Timeout arithmetic must survive counter wrap");

#include "controller_preview.hpp"
constexpr bool preview_checks(){
    const auto p=guitar::encode_preview({0x7ff,65535,true,true,31,0xe0},guitar::UsbMode::RockBand,true,true,0x12345678);
    if(p[5]!=1 || p[6]!=1 || p[8]!=255 || p[9]!=7 || p[10]!=3 || p[11]!=31 || p[12]!=255 || p[14]!=0xe0 || p[19]!=0x12)return false;
    const auto stale=guitar::encode_preview({0x7ff,65535,true,true,31,0xe0},guitar::UsbMode::Hid,true,false,0);
    for(unsigned i=8;i<=15;++i)if(stale[i])return false;
    return stale[6]==0;
}
static_assert(preview_checks(),"Full button/solo/ADC state, source and fresh flag; stale receiver preview is neutral");
