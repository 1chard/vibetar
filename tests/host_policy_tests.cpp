#include "settings_record.hpp"
using namespace guitar;
namespace {
constexpr bool detector_test() {
    HostDetector d;d.reset(0);
    d.request(0xc1,1,0x0100,0,20); // Both hosts ask capabilities.
    if(d.kind()!=HostKind::Unknown || xinput_only({},d.kind(),false)) return false;
    d.os_string();
    if(d.kind()!=HostKind::Windows || xinput_only({},d.kind(),false)) return false;
    d.request(0xc1,0x81,0,3,29);
    if(d.kind()!=HostKind::Xbox360) return false;
    d.reset(0xfffffff0);
    d.request(0xc0,0x20,0,4,40);
    if(d.kind()!=HostKind::Windows) return false;
    d.reset(0);d.request(0xc1,0x81,1,0,29);d.request(0x41,0x81,0,0,29);
    d.request(0xc1,0x81,0x5b17,0x0101,28); // Wrong identity length is not Xbox evidence.
    if(d.kind()!=HostKind::Unknown)return false;
    d.request(0xc1,0x81,0x5b17,0x0101,29);
    return d.kind()==HostKind::Xbox360 && xinput_only({},d.kind(),false);
}
constexpr bool persistence_test() {
    RuntimeConfig c{},r{};c.host={ModePolicy::XInputOnly,true};c.tilt.source=TiltSource::Adxl345;
    auto record=settings::make_record(c,7);
    if(!settings::parse_record(record,r) || !same_config(c,r)) return false;
    record[74]^=1;if(settings::parse_record(record,r)) return false;
    // XFC3 migrates with default Auto policy and preserves existing tilt/extras.
    c.host={};c.extras.digital_pins[0]=0;record=settings::make_record(c,8);
    settings::write_u32(record,0,0x33434658);settings::write_u32(record,72,settings::crc32(record,72));
    if(!settings::parse_record(record,r) || !same_config(c,r)) return false;
    auto p=encode_host(c.host);p[4]=1;if(decode_host(p,r.host)) return false;
    p=encode_host(c.host);p[2]=3;return !decode_host(p,r.host);
}
static_assert(detector_test(),"Positive host fingerprints, reset and no restriction of unknown hosts");
static_assert(persistence_test(),"Host policy persistence / XFC3 migration");
static_assert(policy_boot_mode({ModePolicy::Auto,false},false)==UsbMode::Hid);
static_assert(policy_boot_mode({ModePolicy::Auto,true},false)==UsbMode::Hid);
static_assert(policy_boot_mode({ModePolicy::XInputOnly,true},false)==UsbMode::RockBand);
static_assert(policy_boot_mode({ModePolicy::XInputOnly,true},true)==UsbMode::Hid);
static_assert(policy_boot_mode({ModePolicy::Pc,true},false)==UsbMode::Hid);
static_assert(xinput_only({},HostKind::Xbox360,false) && !xinput_only({},HostKind::Unknown,false));
static_assert(!xinput_only({ModePolicy::Pc},HostKind::Xbox360,false));
static_assert(!xinput_only({ModePolicy::XInputOnly},HostKind::Xbox360,true));
static_assert(policy_next_mode(UsbMode::XInput,true)==UsbMode::RockBand);
static_assert(policy_next_mode(UsbMode::RockBand,true)==UsbMode::XInput);
static_assert(policy_next_mode(UsbMode::RockBand,false)==UsbMode::Hid);
}
