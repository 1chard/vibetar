#pragma once
#include "controller.hpp"
#include <array>
namespace guitar {
enum class ModePolicy : uint8_t { Auto, Pc, XInputOnly };
enum class HostKind : uint8_t { Unknown, Windows, Xbox360 };
struct HostConfig { ModePolicy policy = ModePolicy::Auto; bool boot_rock_band = false; };
using HostPayload = std::array<uint8_t,8>;
inline constexpr uint8_t host_config_report_id=12, host_defaults_report_id=13, host_live_report_id=14;
constexpr HostPayload encode_host(const HostConfig& c, uint8_t status=0) {
    return {1,status,static_cast<uint8_t>(c.policy),static_cast<uint8_t>(c.boot_rock_band)};
}
constexpr bool decode_host(const HostPayload& p, HostConfig& c) {
    if(p[0]!=1 || p[1] || p[2]>2 || p[3]>1) return false;
    for(size_t i=4;i<p.size();++i) if(p[i]) return false;
    c={static_cast<ModePolicy>(p[2]),p[3]!=0};return true;
}
constexpr bool xinput_only(const HostConfig& c, HostKind h, bool recovery) {
    return !recovery && (c.policy==ModePolicy::XInputOnly || (c.policy==ModePolicy::Auto && h==HostKind::Xbox360));
}
constexpr UsbMode policy_next_mode(UsbMode mode, bool limited) {
    return limited ? (mode==UsbMode::XInput ? UsbMode::RockBand : UsbMode::XInput) : next_mode(mode);
}
constexpr UsbMode policy_boot_mode(const HostConfig& c, bool recovery) {
    return recovery || c.policy!=ModePolicy::XInputOnly ? UsbMode::Hid : c.boot_rock_band ? UsbMode::RockBand : UsbMode::XInput;
}
class HostDetector {
public:
    constexpr void reset(uint32_t) { evidence_=0; }
    constexpr void os_string() { evidence_|=1; }
    constexpr void request(uint8_t type,uint8_t request,uint16_t value,uint16_t index,uint16_t length) {
        // Shared capabilities / LED / rumble are not host-identification evidence.
        if(type==0xc0 && request==0x20 && value==0 && index==4 && length) evidence_|=2;
        // XSM3 identification probe, observed in Xbox 360 enumeration.
        // Protocol selector is in the high byte of wIndex; wValue is not zero on retail consoles.
        if(type==0xc1 && request==0x81 && (value==0 || value==0x5b17) && (index&255)<=3 && (index>>8)<=1 && length==29) evidence_|=4;
    }
    constexpr HostKind kind() const { return (evidence_&4) ? HostKind::Xbox360 : (evidence_&3) ? HostKind::Windows : HostKind::Unknown; }
    constexpr uint8_t evidence() const { return evidence_; }
private:
    uint8_t evidence_=0;
};
}
