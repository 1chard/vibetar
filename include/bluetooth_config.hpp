#pragma once
#include <array>
#include <cstdint>
namespace guitar {
inline constexpr uint8_t bluetooth_report_id=21, bluetooth_defaults_report_id=22,
    bluetooth_live_report_id=23, bluetooth_command_report_id=24;
using BluetoothPayload=std::array<uint8_t,8>;
struct BluetoothConfig {
#if VIBETAR_RECEIVER
    bool enabled=true;
#else
    bool enabled=false;
#endif
    bool usb_priority=true;
};
constexpr BluetoothPayload encode_bluetooth(const BluetoothConfig& c,uint8_t status=0) {
    return {1,status,static_cast<uint8_t>(c.enabled),static_cast<uint8_t>(c.usb_priority)};
}
constexpr bool decode_bluetooth(const BluetoothPayload& p,BluetoothConfig& c) {
    if(p[0]!=1 || p[1]!=0 || p[2]>1 || p[3]>1)return false;
    for(unsigned i=4;i<p.size();++i)if(p[i])return false;
    c={p[2]!=0,p[3]!=0};return true;
}
constexpr bool same_bluetooth(const BluetoothConfig& a,const BluetoothConfig& b) {
    return a.enabled==b.enabled && a.usb_priority==b.usb_priority;
}
}
