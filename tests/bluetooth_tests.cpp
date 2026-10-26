#include "settings_record.hpp"
#include "radio_probe.hpp"
using namespace guitar;
namespace {
struct FakeRadio {
    bool initializes=true,responds=true;
    unsigned init_calls=0,query_calls=0,shutdown_calls=0,restore_calls=0;
    constexpr bool init(){++init_calls;return initializes;}
    constexpr bool query(){++query_calls;return responds;}
    constexpr void shutdown(){++shutdown_calls;}
    constexpr void restore(){++restore_calls;}
};
constexpr bool probe_checks(){
    for(unsigned scenario=0;scenario<3;++scenario){
        radio::Probe probe;FakeRadio operations{};
        if(probe.available() || probe.attempted())return false;
        operations.initializes=scenario!=2;operations.responds=scenario==0;
        probe.detect(operations);probe.detect(operations);
        if(!probe.attempted() || probe.available()!=(scenario==0) || operations.init_calls!=1)return false;
        if(operations.query_calls!=(scenario==2?0u:1u))return false;
        if(operations.shutdown_calls!=(scenario==1?1u:0u) || operations.restore_calls!=(scenario==0?0u:1u))return false;
    }
    return true;
}
static_assert(probe_checks(),"One-shot hardware probe, missing chip, failed init and cleanup");
constexpr bool checks(){
    RuntimeConfig config{},decoded{};
    if(config.bluetooth.enabled!=bool(VIBETAR_RECEIVER) || !config.bluetooth.usb_priority)return false;
    config.bluetooth={true,false};config.battery.enabled=true;
    auto r=settings::make_record(config,13);
    if(settings::read_u32(r,0)!=0x37434658 || !settings::parse_record(r,decoded) || !same_config(config,decoded))return false;
    r[114]^=1;if(settings::parse_record(r,decoded))return false;
    auto p=encode_bluetooth(config.bluetooth);p[4]=1;
    if(decode_bluetooth(p,decoded.bluetooth))return false;
    p=encode_bluetooth(config.bluetooth);p[2]=2;
    if(decode_bluetooth(p,decoded.bluetooth))return false;
    p=encode_bluetooth(config.bluetooth);p[1]=1;
    if(decode_bluetooth(p,decoded.bluetooth))return false;
    decoded=config;auto base=encode_config(config);base[15]=0;
    if(!decode_config(base,decoded) || !same_bluetooth(config.bluetooth,decoded.bluetooth))return false;
    // XFC6 upgrades retain battery/settings and default Bluetooth to disabled.
    config.bluetooth={};r=settings::make_record(config,14);
    settings::write_u32(r,0,0x36434658);settings::write_u32(r,112,settings::crc32(r,112));
    return settings::parse_record(r,decoded) && same_config(config,decoded);
}
static_assert(checks(),"Bluetooth persistence, corruption, partial writes and XFC6 migration");
}

#include "ble_feature_transfer.hpp"
namespace {
constexpr bool feature_transfer_checks(){
    using namespace guitar::bluetooth;
    FeatureTransfer transfer{};
    uint8_t bytes[24]{};for(unsigned i=0;i<24;++i)bytes[i]=i;
    if(transfer.append(3,0,bytes,24) || transfer.append(2,23,bytes,2))return false;
    if(!transfer.append(2,18,bytes+18,6) || transfer.complete())return false;
    if(transfer.append(7,0,bytes,18))return false;
    if(!transfer.append(2,0,bytes,18) || !transfer.complete())return false;
    for(unsigned i=0;i<24;++i)if(transfer.data[i]!=i)return false;
    transfer.clear();
    if(transfer.complete() || !transfer.append(21,0,bytes,8) || !transfer.complete())return false;
    transfer.clear();
    return !transfer.append(25,0,bytes,8) && feature_size(6)==30 && feature_size(26)==0;
}
static_assert(feature_transfer_checks(),"Prepared BLE reports: split/out-of-order, bounds, readonly, cancel");
}

namespace {
constexpr bool readonly_transport_writes(){
    guitar::bluetooth::FeatureTransfer t{};uint8_t bytes[30]{};
    if(!t.append(6,0,bytes,18,true) || t.complete())return false;
    if(!t.append(6,18,bytes+18,12,true) || !t.complete())return false;
    t.clear();return !t.append(6,0,bytes,30) && !t.append(26,0,bytes,8,true);
}
static_assert(readonly_transport_writes(),"HIDS SET snapshots are accepted without mutating settings");
}
