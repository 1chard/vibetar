#include "settings_record.hpp"
using namespace guitar;
namespace {
constexpr bool checks(){
    BatteryConfig b{true,3500,150,300};BatteryState s{};
    s.sample(b,1280,true,false,true); // VSYS ~3094 mV, cell estimate ~3394
    if(s.low || !s.valid || s.battery_mv!=3394)return false;
    s.sample(b,1280,true,false,true);if(s.low)return false;
    s.sample(b,1280,true,false,true);if(!s.low)return false;
    s.sample(b,1360,true,false,true);if(!s.low)return false; // hysteresis band
    s.sample(b,1400,true,false,true);if(s.low)return false;
    for(int i=0;i<3;++i)s.sample(b,1280,true,false,true);
    s.sample(b,1280,true,true,true);if(s.low || s.battery_mv || !s.usb)return false;
    for(int i=0;i<3;++i)s.sample(b,1280,true,false,true);
    s.sample(b,0,false,false,false);if(s.low || !s.error || s.valid)return false;
    b.enabled=false;s.sample(b,1280,true,false,true);if(s.low || s.valid || s.error)return false;
    RuntimeConfig c{},r{};c.battery={true,3500,150,300};c.outputs.rules[0]={0,1,true,1};
    if(!valid_config(c) || !settings::parse_record(settings::make_record(c,6),r) || !same_config(c,r))return false;
    if(!output_active(c.outputs.rules[0],UsbMode::RockBand,true) || output_active(c.outputs.rules[0],UsbMode::Hid,false))return false;
    if(output_level(c.outputs.rules[0],UsbMode::XInput,true))return false;
    auto p=encode_battery(c.battery);p[10]=1;if(decode_battery(p,r.battery))return false;
    auto record=settings::make_record(c,7);record[100]^=1;if(settings::parse_record(record,r))return false;
    c.battery={};c.outputs.rules[0].event=0;record=settings::make_record(c,8);
    settings::write_u32(record,0,0x35434658);settings::write_u32(record,96,settings::crc32(record,96));
    return settings::parse_record(record,r) && same_config(c,r);
}
static_assert(checks(),"Battery sampling, USB suppression, validity, hysteresis, LEDs and XFC5 migration");
static_assert(!valid_battery({true,4500,1,0}) && !valid_battery({true,3500,150,1001}));
}
